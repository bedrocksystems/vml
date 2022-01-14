/*
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file
 *  \brief Model for a PL011 device
 */

#include <debug_switches.hpp>
#include <platform/atomic.hpp>
#include <platform/bits.hpp>
#include <platform/mutex.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>
#include <vuart/seq_queue.hpp>
#include <vuart/vuart.hpp>
#include <vuart/vuart_callback.hpp>

namespace Model {
    class Pl011;
    class Irq_controller;
}

/*! \brief Virtual PL011 UART Device
 *
 * The aim of this implementation is to respect the spec while still keeping
 * things simple and fast. To that end, the virtual device has a few differences
 * with a real hardware implementation (while still respecting the spec):
 * - There is no TX queue, in fact, we can pretend that we have a very fast TX
 * queue that transmits characters right away and never gets full.
 * - The notion of speed is not implemented and we always transmit at the maximum
 * speed available which is going to be the speed of copying to memory in practice.
 * - There is no break/parity/framing error emulation. Since we are just copying
 * to and from a memory location, we consider that there is no error in that
 * process for now. Of course, a guest can still query the status of the data transmitted
 * and it will always see that there is no error.
 */
class Model::Pl011 : public Vuart::Vuart {
private:
    // temporary workaround for a cpp2v limitation
    static constexpr char DEVICE_NAME[] = "pl011";

    enum {
        UARTDR = 0x00,
        UARTRSR = 0x04,
        UARTFR = 0x18,
        UARTILPR = 0x20,
        UARTIBRD = 0x24,
        UARTFBRD = 0x28,
        UARTLCR_H = 0x2c,
        UARTCR = 0x30,
        UARTIFLS = 0x34,
        UARTIMSC = 0x38,
        UARTRIS = 0x3c,
        UARTMIS = 0x40,
        UARTICR = 0x44,
        UARTDMACR = 0x48,
        UARTPERIPHID0 = 0xfe0,
        UARTPERIPHID1 = 0xfe4,
        UARTPERIPHID2 = 0xfe8,
        UARTPERIPHID3 = 0xfec,
        UARTPCELLID0 = 0xff0,
        UARTPCELLID1 = 0xff4,
        UARTPCELLID2 = 0xff8,
        UARTPCELLID3 = 0xffc,
    };

    enum {
        CTS = 1 << 0,
        DSR = 1 << 1,
        DCD = 1 << 2,
        BUSY = 1 << 3,
        RXFE = 1 << 4,
        TXFF = 1 << 5,
        RXFF = 1 << 6,
        TXFE = 1 << 7,
        RI = 1 << 8,
    };

    enum {
        BRK = 1 << 0,
        PEN = 1 << 1,
        EPS = 1 << 2,
        STP2 = 1 << 3,
        FEN = 1 << 4,
        WLEN = 1 << 5,
        SPS = 1 << 7,
    };

    enum {
        UARTEN = 1 << 0,
        SIREN = 1 << 1,
        SIRLP = 1 << 2,
        LBE = 1 << 7,
        TXE = 1 << 8,
        RXE = 1 << 9,
        DTR = 1 << 10,
        RTS = 1 << 11,
        OUT1 = 1 << 12,
        OUT2 = 1 << 13,
        RTSEN = 1 << 14,
        CTSEN = 1 << 15,
    };

    static constexpr uint8 RXIFLSEL = 3;
    static constexpr uint8 TXIFLSEL = 0;

    enum FIFOIRQLevel {
        FIFO_1DIV8_FULL = 0b000,
        FIFO_1DIV4_FULL = 0b001,
        FIFO_1DIV2_FULL = 0b010,
        FIFO_3DIV4_FULL = 0b011,
        FIFO_7DIV8_FULL = 0b100,
    };

    enum {
        RIMIM = 1 << 0,
        CTSMIM = 1 << 1,
        DCDMIM = 1 << 2,
        DSRMIM = 1 << 3,
        RXIM = 1 << 4,
        TXIM = 1 << 5,
        RTIM = 1 << 6,
        FEIM = 1 << 7,
        PEIM = 1 << 8,
        BEIM = 1 << 9,
        OEIM = 1 << 10,
    };

    enum {
        RIRMIS = 1 << 0,
        CTSRMIS = 1 << 1,
        DCDRMIS = 1 << 2,
        DSRRMIS = 1 << 3,
        RXRIS = 1 << 4,
        TXRIS = 1 << 5,
        RTRIS = 1 << 6,
        FERIS = 1 << 7,
        PERIS = 1 << 8,
        BERIS = 1 << 9,
        OERIS = 1 << 10,
    };

    uint8 _ilpr;   /*!< IrDA Low power counter register */
    uint16 _ibrd;  /*!< Integer Baud Rate register */
    uint16 _fbrd;  /*!< Fractional Baud Rate register */
    uint16 _lcrh;  /*!< Line control register */
    uint16 _imsc;  /*!< Interrupt Mask Set/Clear register */
    uint16 _cr;    /*!< Control register */
    uint16 _ifls;  /*!< Interrupt FIFO Level select register */
    uint16 _ris;   /*!< Raw interrupt register */
    uint16 _dmacr; /*!< DMA control register */
    bool _rx_irq_disabled_by_icr;
    bool _tx_irq_disabled_by_icr;

    SeqQueue<uint16, 32> _rx_fifo;
    SeqQueue<uint16, 32> _tx_fifo;

    FIFOIRQLevel get_tx_irq_level() const {
        return static_cast<FIFOIRQLevel>(bits_in_range(_ifls, 0, 2));
    }
    FIFOIRQLevel get_rx_irq_level() const {
        return static_cast<FIFOIRQLevel>(bits_in_range(_ifls, 3, 5));
    }

    bool mmio_write(uint64 offset, uint8 access_size, uint64 value);
    bool mmio_read(uint64 offset, uint8 access_size, uint64 &value);
    bool rx_irq_cond() const;
    bool tx_irq_cond() const;

    void set_lvl_to_gicd(bool asserted);
    void updated_irq_lvl_to_gicd_if_needed(bool old_irq_lvl) {
        bool new_irq_lvl = is_irq_asserted();
        if (old_irq_lvl != new_irq_lvl)
            set_lvl_to_gicd(new_irq_lvl);
    }

    bool is_fifo_enabled() const { return FEN & _lcrh; }
    bool can_tx() const { return (_cr & UARTEN) && (_cr & TXE); }
    bool can_rx() const { return (_cr & UARTEN) && (_cr & RXE); }
    bool is_rx_irq_active() const { return _imsc & RXIM; }
    bool is_tx_irq_active() const { return _imsc & TXIM; }
    bool is_rx_irq_asserted() const { return is_rx_irq_active() && (_ris & RXRIS); }
    bool is_tx_irq_asserted() const { return is_tx_irq_active() && (_ris & TXRIS); }
    bool is_irq_asserted() const { return is_rx_irq_asserted() || is_tx_irq_asserted(); }
    void set_rxris(bool b) {
        if (b)
            _ris |= RXRIS;
        else {
            _ris &= static_cast<uint16>(~RXRIS);
            _rx_irq_disabled_by_icr = false;
        }
    }

    void set_txris(bool b) {
        if (b)
            _ris |= TXRIS;
        else {
            _ris &= static_cast<uint16>(~TXRIS);
            _tx_irq_disabled_by_icr = false;
        }
    }

    bool compute_rxris() { return rx_irq_cond() && !_rx_irq_disabled_by_icr; }

    bool compute_txris() { return rx_irq_cond() && !_rx_irq_disabled_by_icr; }

    /*! \brief Send one character to the guest
     *  \param c character to send
     *  \return true is the whole data could be transmitted, false otherwise
     */
    bool write_to_rx_queue(char c);
    bool mmio_write_cr(uint64 value);
    bool mmio_write_ifls(uint64 value);

    void wait_for_available_buffer() { _sig_notify_empty_space.wait(); }

    Irq_controller *_irq_ctlr; /*!< Interrupt controller that will receive interrupts */
    uint16 _irq_id;            /*!< IRQ id when sending an interrupt to the controller */
    Platform::Signal _sig_notify_empty_space; /*!< Synchronize/wait on a buffer that is full */

    /*
     * Rationale on the locking scheme:
     * - The concurrency model can be described as: accesses from the guest and accesses from the
     * outside world that sends characters to the guest.
     * - We need to synchronize the outside world and the guest. That can be done with atomics but
     * it complicates the specification of this code.
     * - Technically, a guest could have several CPUs accessing the pl011 at the same time.
     * Probably, no sane driver would do that but we can still prevent the device to become
     * corrupted by using a global state lock.
     * - Locking should be cheap here, in most cases, only 2 entities will compete for the lock. The
     * outside world will also just wait if the FIFO is full, leaving the guest alone to take the
     * lock.
     * - Overall, performance is not a big concern for virtual UARTs.
     *
     * This could be improved in the future if there is a need.
     */
    Platform::Mutex _state_lock; /*!< Global state lock */

public:
    /*! \brief Constructor for the PL011
     *  \param gic interrupt object that will receive interrupts from the PL011
     *  \param irq IRQ id to use when sending interrupt to the interrupt controller
     */
    Pl011(Irq_controller &irq_ctlr, uint16 const irq)
        : Vuart::Vuart(DEVICE_NAME), _irq_ctlr(&irq_ctlr), _irq_id(irq), _sig_notify_empty_space() {
    }

    // needed for a proof, will be removed soon.
    /*__UNUSED__*/ void delete_fm_register_in_vbus(Vbus::Bus *vb) {
        static_cast<void>(vb->register_device(this, 0, 10));
    }

    bool init(const Platform_ctx *ctx) {
        if (!_sig_notify_empty_space.init(ctx))
            return false;
        if (!_state_lock.init(ctx))
            return false;
        reset(nullptr);
        return true;
    }

    virtual void to_guest(char) override;

    /*! \brief MMIO access function - adhere to the Virtual bus interface
     *  \param access type of access (R/W/X)
     *  \param vctx VCPU context
     *  \param sp Vbus space
     *  \param off offset within the device range
     *  \param bytes size of the access
     *  \param res Input or output value (depending on read or write)
     *  \return status of the access
     */
    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx *vctx, Vbus::Space sp, mword off,
                             uint8 size, uint64 &value) override;

    /*! \brief Reset the PL011 to its initial state
     */
    virtual void reset(const VcpuCtx *) override {
        Platform::MutexGuard guard(&_state_lock);

        _ilpr = 0;
        _ibrd = 0;
        _fbrd = 0;
        _lcrh = 0;
        _imsc = 0;
        _cr = RXE | TXE;
        _ris = 0;
        _ifls = FIFO_1DIV2_FULL << RXIFLSEL | FIFO_1DIV2_FULL << TXIFLSEL;
        _dmacr = 0;

        /*
         * UARTEN is not set by default according to the spec of the pl011.
         * However, some OSes will assume that it was already enabled before they start
         * running (by a bootloader potentially). To be able to get printing and
         * debugging info in this case, we enable the UART when debug is set.
         */
        if (Debug::current_level > Debug::NONE)
            _cr |= UARTEN;

        _rx_fifo.reset(1);
        _sig_notify_empty_space.sig();
    }
};
