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
#include <platform/types.hpp>

Vbus::Err
Model::Pl011::access(Vbus::Access const access, const VcpuCtx *, Vbus::Space, mword const offset,
                     uint8 const size, uint64 &value) {
    bool ok = false;
    Platform::MutexGuard guard(&_state_lock);

    if (access == Vbus::Access::EXEC)
        return Vbus::ACCESS_ERR;

    if (access == Vbus::Access::WRITE)
        ok = mmio_write(offset, size, value);
    else
        ok = mmio_read(offset, size, value);

    return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
}

bool
Model::Pl011::mmio_write(uint64 const offset, uint8 const, uint64 const value) {
    /*
     * All registers are at most 16-bits. Certain are 8-bit according to the spec but
     * unfortunately commonly used OS still generate 32-bit accesses for those so we have
     * to allow this. Note that registers are all 32-bit apart at least.
     */
    // if (size > sizeof(uint32))
    //     WARN("Incorrect size used on acces to the %s: off %llu, size %u, value %llu", name(),
    //          offset, size, value);

    switch (offset) {
    case UARTDR:
        if (_callback != nullptr && can_tx()) {
            const char cval = static_cast<char>(value);
            _callback->from_guest_sent(cval);
        }

        return true;
    case UARTRSR: // We don't emulate errors so nothing to do here
        return true;
    case UARTILPR:
        _ilpr = static_cast<uint8>(value);
        return true;
    case UARTIBRD:
        _ibrd = static_cast<uint16>(value);
        return true;
    case UARTFBRD:
        _fbrd = static_cast<uint8>(value);
        return true;
    case UARTLCR_H:
        _lcrh = static_cast<uint8>(value);

        if (is_fifo_enabled()) {
            _rx_fifo_size = RX_FIFO_MAX_SIZE;
        } else {
            _rx_fifo_size = 1;
            _rx_fifo_chars = _rx_fifo_chars == 0 ? 0 : 1;
            _rx_fifo_ridx = 0;
            _rx_fifo_widx = 0;
        }

        return true;
    case UARTCR: {
        bool could_rx = can_rx();
        _cr = static_cast<uint16>(value);
        if (!could_rx && can_rx())
            _sig_notify_empty_space.sig();
        return true;
    }
    case UARTIFLS:
        _ifls = static_cast<uint16>(value);
        return true;
    case UARTIMSC:
        _imsc = static_cast<uint16>(value);
        return true;
    case UARTICR:
        _ris = _ris & static_cast<uint16>(~(value & 0x7ff));
        if (!(_ris & RXRIS))
            _irq_ctlr->deassert_global_line(_irq_id);

        return true;
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

    return false;
}

bool
Model::Pl011::mmio_read(uint64 const offset, uint8 const size, uint64 &value) {
    if (size > sizeof(uint32))
        return false;

    switch (offset) {
    case UARTDR:
        if (is_fifo_empty() || !can_rx())
            value = 0; // This is an undefined behavior (not specified) returning 0 is fine
        else {
            bool was_full = is_fifo_full();
            value = _rx_fifo[_rx_fifo_ridx++];
            _rx_fifo_ridx %= _rx_fifo_size;
            Barrier::r_before_rw();
            _rx_fifo_chars--;

            if (!should_assert_rx_irq()) {
                _ris &= static_cast<uint16>(~RXRIS);
                _irq_ctlr->deassert_global_line(_irq_id);
            }

            if (was_full)
                _sig_notify_empty_space.sig(); // FIFO is not full anymore, signal the waiter
        }
        return true;
    case UARTRSR: // We don't emulate errors so nothing to do here
        value = 0;
        return true;
    case UARTFR:
        value = (is_fifo_empty() ? RXFE : 0) | (is_fifo_full() ? RXFF : 0) | TXFE;
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
Model::Pl011::should_assert_rx_irq() const {
    if (!is_rx_irq_active())
        return false;

    if (is_fifo_enabled() && is_fifo_full())
        return true;

    uint8 chars = _rx_fifo_chars;

    switch (_ifls) {
    case FIFO_1DIV8_FULL:
        return chars >= 4;
    case FIFO_1DIV4_FULL:
        return chars >= 8;
    case FIFO_1DIV2_FULL:
        return chars >= 16;
    case FIFO_3DIV4_FULL:
        return chars >= 24;
    case FIFO_7DIV8_FULL:
        return chars >= 28;
    }

    return true;
}

bool
Model::Pl011::to_guest(char *buff, uint32 size) {
    Platform::MutexGuard guard(&_state_lock);

    if (is_fifo_full() || !can_rx())
        return false;

    uint32 written;
    for (written = 0; written < size && !is_fifo_full(); written++) {
        _rx_fifo[_rx_fifo_widx++] = static_cast<uint16>(buff[written]);
        _rx_fifo_widx %= _rx_fifo_size;
        Barrier::rw_before_rw();
        _rx_fifo_chars++;
    }

    if (should_assert_rx_irq()) {
        _ris |= RXRIS;
        _irq_ctlr->assert_global_line(_irq_id);
    }

    return size == written;
}
