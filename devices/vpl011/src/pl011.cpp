/**
 * Copyright (C) 2019, 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <arch/barrier.hpp>
#include <model/irq_controller.hpp>
#include <pl011/pl011.hpp>
#include <platform/log.hpp>
#include <platform/types.hpp>

void
Model::Pl011::set_lvl_to_gicd(bool asserted) {
    if (asserted)
        _irq_ctlr->assert_global_line(_irq_id);
    else
        _irq_ctlr->deassert_global_line(_irq_id);
}

Vbus::Err
Model::Pl011::access(Vbus::Access const access, const VcpuCtx *, Vbus::Space, mword const offset,
                     uint8 const size, uint64 &value) {
    bool ok = false;
    Platform::MutexGuard guard(_state_lock);

    if (access == Vbus::Access::EXEC)
        return Vbus::ACCESS_ERR;

    if (access == Vbus::Access::WRITE)
        ok = mmio_write(offset, size, value);
    else
        ok = mmio_read(offset, size, value);

    return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
}

void
warn_bad_access(const char *name, uint64 const offset, uint8 const size, uint64 const value) {
    /*
     * All registers are at most 16-bits. Certain are 8-bit according to the spec but
     * unfortunately commonly used OS still generate 32-bit accesses for those so we have
     * to allow this. Note that registers are all 32-bit apart at least.
     */
    if (size > sizeof(uint32))
        WARN("Incorrect size used on write access to the %s: off %llu, size %u, value %llu", name,
             offset, size, value);
}

bool
Model::Pl011::mmio_write_cr(uint64 const value) {
    bool old_irq = is_irq_asserted();
    bool could_rx = can_rx();
    bool could_tx = can_tx();
    bool old_tx_cond = tx_irq_cond();
    _cr = static_cast<uint16>(value);
    if (!could_rx && can_rx()) // can also check here whether the rx queue has space
        _sig_notify_empty_space.sig();
    if (!could_tx && can_tx()) {
        uint32 num_queued = _tx_fifo.cur_size();
        for (uint32 i = 0; i < num_queued; i++) {
            uint16 val = _tx_fifo.dequeue();
            _callback->from_guest_sent(static_cast<char>(val));
        }
        if (!old_tx_cond) { // TX interrupt condition is true regardless of the watermark level
                            // because the queue is empty
            set_txris(true);
        }
    }
    updated_irq_lvl_to_gicd_if_needed(old_irq);
    return true;
}

bool
Model::Pl011::mmio_write_ifls(uint64 const value) {
    bool old_irq = is_irq_asserted();
    bool old_tx_cond = tx_irq_cond();
    bool old_rx_cond = tx_irq_cond();
    _ifls = static_cast<uint16>(value);
    bool new_rx_cond = rx_irq_cond();
    bool new_tx_cond = tx_irq_cond();
    if (!new_tx_cond)
        set_txris(false);
    else if (!old_tx_cond)
        set_txris(true);
    if (!new_rx_cond)
        set_rxris(false);
    else if (!old_rx_cond)
        set_rxris(true);
    updated_irq_lvl_to_gicd_if_needed(old_irq);
    return true;
}

bool
Model::Pl011::mmio_write_icr(uint64 const value) {
    bool old_irq = is_irq_asserted();
    uint16 oldris = _ris;
    _ris = _ris & static_cast<uint16>(~(value & 0x7ff));
    updated_irq_lvl_to_gicd_if_needed(old_irq);

    if ((value & TXRIS) & (oldris & TXRIS)) {
        // the next time txris is computed, it would be false until the tx interrupt
        // condition makes the transition from being false to being true. see set_txris(false)
        // and compute_txris()
        _tx_irq_disabled_by_icr = true;
    }
    if ((value & RXRIS) & (oldris & RXRIS)) {
        _rx_irq_disabled_by_icr = true;
    }
    return true;
}

bool
Model::Pl011::mmio_write(uint64 const offset, uint8 const size, uint64 const value) {
    warn_bad_access(name(), offset, size, value);

    switch (offset) {
    case UARTDR: {
        ASSERT(_callback != nullptr);
        bool old_irq = is_irq_asserted();
        if (can_tx()) {
            _callback->from_guest_sent(
                static_cast<char>(value)); // queue length remains same (0), so no interrupt change.
            // one can argue that queue length becomes 1 transiently, but 1 and 0 are equiv upto
            // watermark conditions
        } else {
            _tx_fifo.enqueue(static_cast<unsigned char>(value));
        }
        set_txris(tx_irq_cond());
        updated_irq_lvl_to_gicd_if_needed(old_irq);

        return true;
    }
    case UARTRSR: // READ only register
        return true;
    case UARTILPR:
        _ilpr = static_cast<uint8>(value);
        return true;
    case UARTIBRD:
        _ibrd = static_cast<uint16>(value); // change the model: the emulated model will store the
                                            // baud rate, but always transmit at a constant
        return true;
    case UARTFBRD:
        _fbrd = static_cast<uint8>(value); // also baud rate
        return true;
    case UARTLCR_H:
        _lcrh = static_cast<uint8>(value);

        // add this to the model
        if (is_fifo_enabled()) {
            _rx_fifo.reset_maximize_capacity();
            _tx_fifo.reset_maximize_capacity();
        } else {
            _rx_fifo.reset(1);
            _tx_fifo.reset(1);
        }
        // recheck for interrupts?

        return true;
    case UARTCR:
        return mmio_write_cr(value);
    case UARTIFLS:
        return mmio_write_ifls(value);
    case UARTIMSC: {
        bool old_irq = is_irq_asserted();
        _imsc = static_cast<uint16>(value); // check again for interrupts
        updated_irq_lvl_to_gicd_if_needed(old_irq);
        return true;
    }

    case UARTICR:
        return mmio_write_icr(value);

    case UARTDMACR:
        _dmacr = static_cast<uint16>(value);
        return true;
    /* Read only registers - Write ignored */
    case UARTFR:
    case UARTRIS:
    case UARTMIS:
    case UARTPERIPHID0:
    case UARTPERIPHID1:
    case UARTPERIPHID2:
    case UARTPERIPHID3:
    case UARTPCELLID0:
    case UARTPCELLID1:
    case UARTPCELLID2:
    case UARTPCELLID3:
        return true;
    }

    return true;
}

bool
Model::Pl011::mmio_read(uint64 const offset, uint8 const size, uint64 &value) {
    if (size > sizeof(uint32))
        WARN("Incorrect size used on read access to the %s: off %llu, size %u, value %llu", name(),
             offset, size, value);

    switch (offset) {
    case UARTDR:
        if (_rx_fifo.is_empty() || !can_rx()) // drop can_rx()? do litmus test. fill up RX fifo then
                                              // disable RXE then read
            value = 0; // This is an undefined behavior (not specified) returning 0 is fine
        else {
            bool was_full = _rx_fifo.is_full();
            bool prev_rx_cond = rx_irq_cond();
            value = _rx_fifo.dequeue();

            if (prev_rx_cond && !rx_irq_cond()) {
                set_rxris(false);
                _irq_ctlr->deassert_global_line(_irq_id);
            }

            if (was_full)
                _sig_notify_empty_space.sig(); // FIFO is not full anymore, signal the waiter (umux)
        }
        return true;
    case UARTRSR: // We don't emulate errors so nothing to do here
        value = 0;
        return true;
    case UARTFR:
        value = (_rx_fifo.is_empty() ? RXFE : 0) | (_rx_fifo.is_full() ? RXFF : 0) | TXFE;
        return true;
    case UARTILPR:
        value = _ilpr;
        return true;
    case UARTIBRD:
        value = _ibrd;
        return true;
    case UARTFBRD:
        value = _fbrd;
        return true;
    case UARTLCR_H:
        value = _lcrh;
        return true;
    case UARTCR:
        value = _cr;
        return true;
    case UARTIFLS:
        value = _ifls;
        return true;
    case UARTIMSC:
        value = _imsc;
        return true;
    case UARTRIS:
        value = _ris;
        return true;
    case UARTMIS:
        // An IMSC bit=1 means the corresponding interrupt is enabled (unmasked).
        value = _ris & _imsc;
        return true;
    case UARTICR: // Write only, we can ignore reads
        return true;
    case UARTDMACR:
        value = _dmacr;
        return true;
    case UARTPERIPHID0:
        value = 0x11;
        return true;
    case UARTPERIPHID1:
        value = 0x10;
        return true;
    case UARTPERIPHID2:
        value = 0x14;
        return true;
    case UARTPERIPHID3:
        value = 0x0;
        return true;
    case UARTPCELLID0:
        value = 0xd;
        return true;
    case UARTPCELLID1:
        value = 0xf0;
        return true;
    case UARTPCELLID2:
        value = 0x5;
        return true;
    case UARTPCELLID3:
        value = 0xb1;
        return true;
    }

    return false;
}

bool
Model::Pl011::rx_irq_cond() const {
    uint32 chars = _rx_fifo.cur_size();
    return (chars > 0);

    /* The manual says: the receive timeout interrupt is asserted when the receive FIFO is not
     * empty, and no further data is received over a 32-bit period. The receive timeout interrupt is
     * cleared either when the FIFO becomes empty through reading all the data (or by reading the
     * holding register), or when a 1 is written to the corresponding bit of the UARTICR register.
     *
     * Because timing is irrelvant for vmm, we assume that we are already over the 32-bit period.
     */
    // switch (get_rx_irq_level()) {
    // case FIFO_1DIV8_FULL:
    //     return chars >= 4;
    // case FIFO_1DIV4_FULL:
    //     return chars >= 8;
    // case FIFO_1DIV2_FULL:
    //     return chars >= 16;
    // case FIFO_3DIV4_FULL:
    //     return chars >= 24;
    // case FIFO_7DIV8_FULL:
    //     return chars >= 28;
    // }

    // return true;
}

bool
Model::Pl011::tx_irq_cond() const {
    uint32 chars = _tx_fifo.cur_size();

    switch (get_tx_irq_level()) {
    case FIFO_1DIV8_FULL:
        return chars <= 4;
    case FIFO_1DIV4_FULL:
        return chars <= 8;
    case FIFO_1DIV2_FULL:
        return chars <= 16;
    case FIFO_3DIV4_FULL:
        return chars <= 24;
    case FIFO_7DIV8_FULL:
        return chars <= 28;
    }

    return true;
}

bool
Model::Pl011::write_to_rx_queue(char c) {
    Platform::MutexGuard guard(_state_lock);

    if (_rx_fifo.is_full() || !can_rx())
        return false;

    bool old_irq = is_irq_asserted();
    _rx_fifo.enqueue(static_cast<uint8>(c));

    if (compute_rxris()) { // we know this would be true. can drop the check
        set_rxris(true);
        updated_irq_lvl_to_gicd_if_needed(old_irq);
    }

    return true;
}

void
Model::Pl011::to_guest(char c) {
    while (!write_to_rx_queue(c))
        wait_for_available_buffer();
}
