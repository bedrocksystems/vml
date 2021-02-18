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

#include <platform/atomic.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>
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

    enum {
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

    static constexpr uint8 RX_FIFO_MAX_SIZE = 16;
    uint8 _rx_fifo_size{1};            /*!< Maximum configured size */
    atomic<uint8> _rx_fifo_chars{0};   /*!< Current number of chars in the FIFO */
    uint8 _rx_fifo_ridx{0};            /*!< Read index in the FIFO */
    uint8 _rx_fifo_widx{0};            /*!< Write index in the FIFO */
    uint16 _rx_fifo[RX_FIFO_MAX_SIZE]; /*!< Receive FIFO */

    bool mmio_write(uint64 const offset, uint8 const access_size, uint64 const value);
    bool mmio_read(uint64 const offset, uint8 const access_size, uint64 &value);
    bool should_assert_rx_irq() const;

    bool is_fifo_enabled() const { return FEN & _lcrh; }
    bool is_fifo_empty() const { return _rx_fifo_chars == 0; }
    bool is_fifo_full() const { return _rx_fifo_chars == _rx_fifo_size; }
    bool can_tx() const { return (_cr & UARTEN) && (_cr & TXE); }
    bool can_rx() const { return (_cr & UARTEN) && (_cr & RXE); }
    bool is_rx_irq_active() const { return _imsc & RXIM; }

    Irq_controller *_irq_ctlr; /*!< Interrupt controller that will receive interrupts */
    uint16 _irq_id;            /*!< IRQ id when sending an interrupt to the controller */
    Platform::Signal _sig_notify_empty_space; /*!< Synchronize/wait on a buffer that is full */

public:
    /*! \brief Constructor for the PL011
     *  \param gic interrupt object that will receive interrupts from the PL011
     *  \param irq IRQ id to use when sending interrupt to the interrupt controller
     */
    Pl011(Irq_controller &irq_ctlr, uint16 const irq)
        : Vuart::Vuart("pl011"), _irq_ctlr(&irq_ctlr), _irq_id(irq), _sig_notify_empty_space() {}

    bool init(const Platform_ctx *ctx) {
        if (!_sig_notify_empty_space.init(ctx))
            return false;
        reset(nullptr);
        return true;
    }

    /*! \brief Send characters to the guest
     *  \param buff Buffer containing the characters
     *  \param size Number of characters to read from the buffer
     *  \return true is the whole data could be transmitted, false otherwise
     */
    virtual bool to_guest(char *buff, uint32 size) override;

    virtual void wait_for_available_buffer() override { _sig_notify_empty_space.wait(); }

    /*! \brief MMIO access function - adhere to the Virtual bus interface
     *  \param access type of access (R/W/X)
     *  \param vctx VCPU context
     *  \param sp Vbus space
     *  \param off offset within the device range
     *  \param bytes size of the access
     *  \param res Input or output value (depending on read or write)
     *  \return status of the access
     */
    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx *vctx, Vbus::Space sp, mword off,
                             uint8 bytes, uint64 &res) override;

    /*! \brief Reset the PL011 to its initial state
     */
    virtual void reset(const Vcpu_ctx *) override {
        _ilpr = 0;
        _ibrd = 0;
        _fbrd = 0;
        _lcrh = 0;
        _imsc = 0;
        _cr = RXE | TXE;
        _ris = 0;
        _ifls = FIFO_1DIV2_FULL << RXIFLSEL | FIFO_1DIV2_FULL << TXIFLSEL;
        _dmacr = 0;

        for (uint8 i = 0; i < RX_FIFO_MAX_SIZE; i++)
            _rx_fifo[i] = 0; // Reset error status to zero

        _rx_fifo_size = 1;
        _rx_fifo_chars = 0;
        _rx_fifo_ridx = 0;
        _rx_fifo_widx = 0;
        _sig_notify_empty_space.sig();
    }
};
