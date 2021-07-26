/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/irq_controller.hpp>
#include <model/virtio_common.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <platform/types.hpp>
#include <platform/virtqueue.hpp>

namespace Virtio {
    class Console;
    class Device;
    class Virtio_console;
};

class Virtio::Device : public Vbus::Device {
protected:
    Model::Irq_controller *const _irq_ctlr;
    Vbus::Bus const *const _vbus;
    uint16 const _irq;
    Virtio::DeviceState _dev_state;
    Virtio::Transport *_transport;

    Virtio::QueueState &queue(uint8 index) { return _dev_state.queue[index]; }
    Virtio::QueueData const &queue_data(uint8 index) const { return _dev_state.data[index]; }
    Virtio::DeviceQueue &device_queue(uint8 index) { return queue(index).device_queue(); }

    void reset_virtio() { _dev_state.reset(); }

    void assert_irq() {
        // Currently there is no run-time config change. We'll need to inject config change
        // interrupts when we start supporting run-time config updates.
        _transport->assert_queue_interrupt(_irq_ctlr, _irq, _dev_state);
    }

    void deassert_irq() {
        // Currently there is no run-time config change. We'll need to handle config change
        // interrupts when we start supporting run-time config updates.
        _transport->deassert_queue_interrupt(_irq_ctlr, _irq, _dev_state);
    }

    void update_config_gen() { _dev_state.update_config_gen(); }

    void handle_events() {
        if (_dev_state.construct_queue) {
            _dev_state.construct_queue = false;
            _dev_state.construct_selected(*_vbus);
        }

        if (_dev_state.status_changed) {
            _dev_state.status_changed = false;

            if (_dev_state.status == static_cast<uint32>(Virtio::DeviceStatus::DEVICE_RESET)) {
                _dev_state.reset();
            } else if (_dev_state.status & static_cast<uint32>(Virtio::DeviceStatus::DRIVER_OK)) {
                driver_ok();
            }
        }

        if (_dev_state.notify) {
            _dev_state.notify = false;
            notify(_dev_state.notify_val);
        }
    }

    virtual Vbus::Err access(Vbus::Access const access, const VcpuCtx *, Vbus::Space,
                             mword const offset, uint8 const size, uint64 &value) override {

        bool ok = _transport->access(access, offset, size, value, _dev_state);
        if (ok)
            handle_events();

        return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
    }

    virtual void notify(uint32) = 0;
    virtual void driver_ok() = 0;

public:
    Device(const char *name, Virtio::DeviceID device_id, const Vbus::Bus &bus,
           Model::Irq_controller &irq_ctlr, void *config_space, uint32 config_size,
           uint16 const irq, uint16 const queue_num, Virtio::Transport *transport,
           uint32 const device_feature_lower = 0)
        : Vbus::Device(name), _irq_ctlr(&irq_ctlr), _vbus(&bus), _irq(irq),
          _dev_state(queue_num, 0x554d4551ULL, static_cast<uint32>(device_id), device_feature_lower,
                     config_space, config_size),
          _transport(transport) {}

    uint64 drv_feature() const {
        return (uint64(_dev_state.drv_feature_upper) << 32) | _dev_state.drv_feature_lower;
    }
};

class Virtio::Virtio_console {
public:
    Virtio_console() {}

    virtual bool to_guest(const char *, uint32) = 0;
    virtual void wait_for_available_buffer() = 0;

    virtual const char *from_guest(uint32 &size) = 0;
    virtual void release_buffer() = 0;

    void register_callback(Virtio::Callback *callback) { _callback = callback; }

protected:
    Virtio::Callback *_callback{nullptr};
};
