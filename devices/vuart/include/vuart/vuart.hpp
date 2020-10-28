/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>
#include <vbus/vbus.hpp>
#include <vuart/vuart_callback.hpp>

namespace Vuart {
    class Vuart;
    class Dummy;
}

class Vuart::Vuart : public Vbus::Device {
public:
    Vuart() : Vbus::Device("vUART") {}
    Vuart(const char *name) : Vbus::Device(name) {}

    /*! \brief Send characters to the guest
     *  \param buff Buffer containing the characters
     *  \param size Number of characters to read from the buffer
     *  \return true is the whole data could be transmitted, false otherwise
     */
    virtual bool to_guest(char *, uint32) { return true; }

    /*! \brief Register the callback handler to send data outside
     *  \param callback The callback object
     */
    void register_callback(::Vuart::Tx_callback *callback) { _callback = callback; }

protected:
    ::Vuart::Tx_callback *_callback{nullptr}; /*!< Callback interface used to send chars (TX) */
};

/*! \brief Virtual 'Dummy' UART.
 *
 * This class does not implement any existing UART but can be used to emulate
 * a very basic UART in polling mode if configured correctly.
 */
class Vuart::Dummy : public Vuart::Vuart {
public:
    Dummy(const char *name, uint64 write_off, uint64 read_default_value)
        : Vuart(name), _write_off(write_off), _read_default_value(read_default_value) {}

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx *, Vbus::Space, mword off, uint8,
                             uint64 &value) override {
        if (access != Vbus::READ && access != Vbus::WRITE)
            return Vbus::ACCESS_ERR;

        if (access == Vbus::READ)
            value = _read_default_value;
        else if (off == _write_off && _callback != nullptr) {
            const char cval = static_cast<char>(value);
            _callback->from_guest_sent(cval);
        }

        return Vbus::OK;
    }

    virtual void reset() override {}

private:
    const uint64 _write_off;
    const uint64 _read_default_value;
};