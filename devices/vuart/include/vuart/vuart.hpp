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
    explicit Vuart(const char *name) : Vbus::Device(name) {}

    /*! \brief Send characters to the guest. May block when the UART internal buffer is full.
     *  \param c character to send to the guest
     *  \pre valid Vuart object
     *  \post same as pre - characters have been sent to the guest (could be best effort)
     */
    virtual void to_guest(char) { return; }

    /*! \brief Register the callback handler to send data outside
     *  \param callback The callback object
     *  \pre Valid Vuart object, no callback set, q ownership of the callback object
     *  \post Valid Vuart object wit the callback set (keeps ownership of the callback)
     */
    void register_callback(::Vuart::Tx_callback *callback,
                           ::Vuart::LifeCycleCallbacks *lifecycle_callbacks) {
        _callback = callback;
        _lifecycle_callbacks = lifecycle_callbacks;
    }

protected:
    ::Vuart::Tx_callback *_callback{nullptr}; /*!< Callback interface used to send chars (TX) */
    ::Vuart::LifeCycleCallbacks *_lifecycle_callbacks{
        nullptr}; /*!< Callback interface used for lifecycle */
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

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx *, Vbus::Space, mword off, uint8,
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

    virtual void reset(const VcpuCtx *) override {}

private:
    const uint64 _write_off;
    const uint64 _read_default_value;
};
