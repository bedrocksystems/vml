/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/irq_controller.hpp>
#include <model/simple_as.hpp>
#include <model/virtio_common.hpp>
#include <model/virtio_sg.hpp>
#include <model/virtqueue.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <platform/types.hpp>

namespace Virtio {
    class Console;
    class Device;
};

class Virtio::Device : public Vbus::Device, public Virtio::Sg::Buffer::ChainAccessor {
private:
    static constexpr uint32 VENDOR_ID = 0x20564842ULL; // little-endian "BHV "

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
            } else if ((_dev_state.status & static_cast<uint32>(Virtio::DeviceStatus::DRIVER_OK)) != 0u) {
                driver_ok();
            }
        }

        if (_dev_state.irq_acknowledged) {
            _dev_state.irq_acknowledged = false;
            deassert_irq();
        }

        if (_dev_state.notify) {
            _dev_state.notify = false;
            notify(_dev_state.notify_val);
        }
    }

    Vbus::Err access(Vbus::Access const access, const VcpuCtx *, Vbus::Space, mword const offset, uint8 const size,
                     uint64 &value) override {

        bool ok = _transport->access(access, offset, size, value, _dev_state);
        if (ok)
            handle_events();

        return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
    }

    virtual void notify(uint32) = 0;
    virtual void driver_ok() = 0;

public:
    Device(const char *name, Virtio::DeviceID device_id, const Vbus::Bus &bus, Model::Irq_controller &irq_ctlr,
           void *config_space, uint32 config_size, uint16 const irq, uint16 const queue_num, Virtio::Transport *transport,
           uint64 const device_feature = 0)
        : Vbus::Device(name), _irq_ctlr(&irq_ctlr), _vbus(&bus), _irq(irq),
          _dev_state(queue_num, VENDOR_ID, static_cast<uint32>(device_id), device_feature, config_space, config_size),
          _transport(transport) {}

    uint64 drv_feature() const { return combine_low_high(_dev_state.drv_feature_lower, _dev_state.drv_feature_upper); }

    // [GuestPhysicalToVirtual] overrides inherited by [Virtio::Sg::Buffer::ChainAccessor]
    //
    // NOTE: mapping depends on whether or not the the va will be used for reads or writes,
    // but unmapping is done unconditionally.
    Errno gpa_to_va(const GPA &gpa, size_t size_bytes, char *&va) override {
        char *mapped_va = Model::SimpleAS::map_guest_mem(*_vbus, gpa, size_bytes, false);

        if (nullptr == mapped_va) {
            return Errno::NOENT;
        }

        va = mapped_va;
        return Errno::NONE;
    }
    Errno gpa_to_va_write(const GPA &gpa, size_t size_bytes, char *&va) override {
        char *mapped_va = Model::SimpleAS::map_guest_mem(*_vbus, gpa, size_bytes, true);

        if (nullptr == mapped_va) {
            return Errno::NOENT;
        }

        va = mapped_va;
        return Errno::NONE;
    }
    Errno gpa_to_va_post(const GPA &, size_t size_bytes, char *va) override {
        Model::SimpleAS::unmap_guest_mem(va, size_bytes);
        return Errno::NONE;
    }

    Errno deinit() override { return Errno::NONE; }
};
