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
    uint64 *_config_space{nullptr};
    uint32 _config_size;

    uint16 const _irq;
    uint16 const _queue_num_max; /* spec says: must be power of 2, max 32768 */
    uint8 const _device_id;
    uint32 const _device_feature_lower;

    uint32 _sel_queue{0};
    uint32 _vendor_id{0x554d4551ULL /* QEMU */};
    uint32 _irq_status{0};
    uint32 _status{0};
    uint32 _drv_device_sel{0};
    uint32 _drv_feature_sel{0};
    uint32 _drv_feature_upper{0};
    uint32 _drv_feature_lower{0};
    uint32 _config_generation{0};

    QueueData _data[static_cast<uint8>(Virtio::Queues::MAX)];
    QueueState _queue[static_cast<uint8>(Virtio::Queues::MAX)];

    enum {
        RO_MAGIC = 0x0,
        RO_MAGIC_END = 0x3,
        RO_VERSION = 0x4,
        RO_VERSION_END = 0x7,
        RO_DEVICE_ID = 0x8,
        RO_DEVICE_ID_END = 0xb,
        RO_VENDOR_ID = 0xc,
        RO_VENDOR_ID_END = 0xf,
        RO_DEVICE_FEATURE = 0x10,
        RO_DEVICE_FEATURE_END = 0x13,
        RW_DEVICE_FEATURE_SEL = 0x14,
        RW_DEVICE_FEATURE_SEL_END = 0x17,

        WO_DRIVER_FEATURE = 0x20,
        WO_DRIVER_FEATURE_END = 0x23,
        RW_DRIVER_FEATURE_SEL = 0x24,
        RW_DRIVER_FEATURE_SEL_END = 0x27,

        WO_QUEUE_SEL = 0x30,
        WO_QUEUE_SEL_END = 0x33,
        RO_QUEUE_NUM_MAX = 0x34,
        RO_QUEUE_NUM_MAX_END = 0x37,
        WO_QUEUE_NUM = 0x38,
        WO_QUEUE_NUM_END = 0x3b,

        RW_QUEUE_READY = 0x44,
        RW_QUEUE_READY_END = 0x47,

        WO_QUEUE_NOTIFY = 0x50,
        RO_IRQ_STATUS = 0x60,
        RO_IRQ_STATUS_END = 0x63,
        WO_IRQ_ACK = 0x64,
        WO_IRQ_ACK_END = 0x67,

        RW_STATUS = 0x70,
        RW_STATUS_END = 0x73,

        WO_QUEUE_DESCR_LOW = 0x80,
        WO_QUEUE_DESCR_LOW_END = 0x83,
        WO_QUEUE_DESCR_HIGH = 0x84,
        WO_QUEUE_DESCR_HIGH_END = 0x87,

        WO_QUEUE_DRIVER_LOW = 0x90,
        WO_QUEUE_DRIVER_LOW_END = 0x93,
        WO_QUEUE_DRIVER_HIGH = 0x94,
        WO_QUEUE_DRIVER_HIGH_END = 0x97,

        WO_QUEUE_DEVICE_LOW = 0xa0,
        WO_QUEUE_DEVICE_LOW_END = 0xa3,
        WO_QUEUE_DEVICE_HIGH = 0xa4,
        WO_QUEUE_DEVICE_HIGH_END = 0xa7,

        RO_CONFIG_GENERATION = 0xfc,
        RO_CONFIG_GENERATION_END = 0xff,

        RW_CONFIG = 0x100,
        RW_CONFIG_END = 0x163,
    };

    QueueData const &queue_data() const { return _data[_sel_queue]; }
    QueueData &queue_data() { return _data[_sel_queue]; }

    void queue_state(bool const construct) {
        if (construct && !_queue[_sel_queue].constructed()) {
            _queue[_sel_queue].construct(queue_data(), *_vbus);
        }

        if (!construct && _queue[_sel_queue].constructed()) {
            _queue[_sel_queue].destruct();
        }
    }

    void reset_virtio() {
        for (int i = 0; i < static_cast<uint8>(Virtio::Queues::MAX); i++) {
            _queue[i].destruct();
            _data[i] = {};
        }

        _status = 0;
        _irq_status = 0;
        _drv_device_sel = 0;
        _drv_feature_sel = 0;
        _drv_feature_upper = 0;
        _drv_feature_lower = 0;
    }

    void assert_irq() {
        _irq_status = 0x1;
        _irq_ctlr->assert_global_line(_irq);
    }

    void deassert_irq() {
        _irq_status = 0;
        _irq_ctlr->deassert_global_line(_irq);
    }

    void update_config_gen() { __atomic_fetch_add(&_config_generation, 1, __ATOMIC_SEQ_CST); }

    bool read(uint64 const offset, uint8 const bytes, uint64 &value) const {
        if (bytes > 4)
            return false;

        switch (offset) {
        case RO_MAGIC ... RO_MAGIC_END:
            return read_register(offset, RO_MAGIC, RO_MAGIC_END, bytes, 0x74726976ULL, value);
        case RO_VERSION ... RO_VERSION_END:
            return read_register(offset, RO_VERSION, RO_VERSION_END, bytes, 2, value);
        case RO_DEVICE_ID ... RO_DEVICE_ID_END:
            return read_register(offset, RO_DEVICE_ID, RO_DEVICE_ID_END, bytes, _device_id, value);
        case RO_VENDOR_ID ... RO_VENDOR_ID_END:
            return read_register(offset, RO_VENDOR_ID, RO_VENDOR_ID_END, bytes, _vendor_id, value);
        case RO_DEVICE_FEATURE ... RO_DEVICE_FEATURE_END:
            if (_drv_device_sel == 0)
                return read_register(offset, RO_DEVICE_FEATURE, RO_DEVICE_FEATURE_END, bytes,
                                     _device_feature_lower, value);
            else
                return read_register(offset, RO_DEVICE_FEATURE, RO_DEVICE_FEATURE_END, bytes,
                                     1 /*VIRTIO_F_VERSION_1*/, value);
        case RW_DEVICE_FEATURE_SEL ... RW_DEVICE_FEATURE_SEL_END:
            return read_register(offset, RW_DEVICE_FEATURE_SEL, RW_DEVICE_FEATURE_SEL_END, bytes,
                                 _drv_device_sel, value);
        case RW_DRIVER_FEATURE_SEL ... RW_DRIVER_FEATURE_SEL_END:
            return read_register(offset, RW_DRIVER_FEATURE_SEL, RW_DRIVER_FEATURE_SEL_END, bytes,
                                 _drv_feature_sel, value);
        case RO_QUEUE_NUM_MAX ... RO_QUEUE_NUM_MAX_END:
            return read_register(offset, RO_QUEUE_NUM_MAX, RO_QUEUE_NUM_MAX_END, bytes,
                                 _queue_num_max, value);
        case RW_QUEUE_READY ... RW_QUEUE_READY_END:
            return read_register(offset, RW_QUEUE_READY, RW_QUEUE_READY_END, bytes,
                                 queue_data().ready, value);
        case RO_IRQ_STATUS ... RO_IRQ_STATUS_END:
            return read_register(offset, RO_IRQ_STATUS, RO_IRQ_STATUS_END, bytes, _irq_status,
                                 value);
        case RW_STATUS ... RW_STATUS_END:
            return read_register(offset, RW_STATUS, RW_STATUS_END, bytes, _status, value);

        case RO_CONFIG_GENERATION ... RO_CONFIG_GENERATION_END:
            return read_register(offset, RO_CONFIG_GENERATION, RO_CONFIG_GENERATION_END, bytes,
                                 __atomic_load_n(&_config_generation, __ATOMIC_SEQ_CST), value);

        // Config space access can be byte aligned.
        case RW_CONFIG ... RW_CONFIG_END:
            return read_register(offset, RW_CONFIG, (RW_CONFIG + _config_size - 1), bytes,
                                 _config_space[(offset & ~(8ULL - 1)) - RW_CONFIG], value);
        }
        return false;
    }

    bool write(uint64 const offset, uint8 const bytes, uint64 const value) {
        if (bytes > 4)
            return false;

        switch (offset) {
        case RW_DEVICE_FEATURE_SEL ... RW_DEVICE_FEATURE_SEL_END:
            return write_register(offset, RW_DEVICE_FEATURE_SEL, RW_DEVICE_FEATURE_SEL_END, bytes,
                                  value, _drv_device_sel);
        case WO_DRIVER_FEATURE ... WO_DRIVER_FEATURE_END:
            if (_drv_feature_sel == 0)
                return write_register(offset, WO_DRIVER_FEATURE, WO_DRIVER_FEATURE_END, bytes,
                                      value, _drv_feature_lower);
            else
                return write_register(offset, WO_DRIVER_FEATURE, WO_DRIVER_FEATURE_END, bytes,
                                      value, _drv_feature_upper);
        case RW_DRIVER_FEATURE_SEL ... RW_DRIVER_FEATURE_SEL_END:
            return write_register(offset, RW_DRIVER_FEATURE_SEL, RW_DRIVER_FEATURE_SEL_END, bytes,
                                  value, _drv_feature_sel);
        case WO_QUEUE_SEL ... WO_QUEUE_SEL_END:
            if (value >= static_cast<uint8>(Virtio::Queues::MAX))
                return true; /* ignore out of bound */
            return write_register(offset, WO_QUEUE_SEL, WO_QUEUE_SEL_END, bytes, value, _sel_queue);
        case WO_QUEUE_NUM ... WO_QUEUE_NUM_END:
            if (value > _queue_num_max)
                return true; /* ignore out of bound */
            return write_register(offset, WO_QUEUE_NUM, WO_QUEUE_NUM_END, bytes, value,
                                  queue_data().num);
        case RW_QUEUE_READY ... RW_QUEUE_READY_END: {
            if (!write_register(offset, RW_QUEUE_READY, RW_QUEUE_READY_END, bytes, value,
                                queue_data().ready))
                return false;
            queue_state(value == 1);
            return true;
        }
        case WO_IRQ_ACK ... WO_IRQ_ACK_END:
            return true;
        case RW_STATUS ... RW_STATUS_END:
            if (value == static_cast<uint32>(Virtio::DeviceStatus::DEVICE_RESET))
                reset_virtio();
            else if (value & static_cast<uint32>(Virtio::DeviceStatus::DRIVER_OK))
                driver_ok();
            return write_register(offset, RW_STATUS, RW_STATUS_END, bytes, value, _status);
        case WO_QUEUE_NOTIFY:
            notify(uint32(value));
            return true;
        case WO_QUEUE_DESCR_LOW ... WO_QUEUE_DESCR_LOW_END:
            return write_register(offset, WO_QUEUE_DESCR_LOW, WO_QUEUE_DESCR_LOW_END, bytes, value,
                                  queue_data().descr_low);
        case WO_QUEUE_DESCR_HIGH ... WO_QUEUE_DESCR_HIGH_END:
            return write_register(offset, WO_QUEUE_DESCR_HIGH, WO_QUEUE_DESCR_HIGH_END, bytes,
                                  value, queue_data().descr_high);
        case WO_QUEUE_DRIVER_LOW ... WO_QUEUE_DRIVER_LOW_END:
            return write_register(offset, WO_QUEUE_DRIVER_LOW, WO_QUEUE_DRIVER_LOW_END, bytes,
                                  value, queue_data().driver_low);
        case WO_QUEUE_DRIVER_HIGH ... WO_QUEUE_DRIVER_HIGH_END:
            return write_register(offset, WO_QUEUE_DRIVER_HIGH, WO_QUEUE_DRIVER_HIGH_END, bytes,
                                  value, queue_data().driver_high);
        case WO_QUEUE_DEVICE_LOW ... WO_QUEUE_DEVICE_LOW_END:
            return write_register(offset, WO_QUEUE_DEVICE_LOW, WO_QUEUE_DEVICE_LOW_END, bytes,
                                  value, queue_data().device_low);
        case WO_QUEUE_DEVICE_HIGH ... WO_QUEUE_DEVICE_HIGH_END:
            return write_register(offset, WO_QUEUE_DEVICE_HIGH, WO_QUEUE_DEVICE_HIGH_END, bytes,
                                  value, queue_data().device_high);
        // Config space access can be byte aligned.
        case RW_CONFIG ... RW_CONFIG_END:
            return write_register(offset, RW_CONFIG, (RW_CONFIG + _config_size - 1), bytes, value,
                                  _config_space[(offset & ~(8ULL - 1)) - RW_CONFIG]);
        }
        return false;
    }

    virtual Vbus::Err access(Vbus::Access const access, const VcpuCtx *, Vbus::Space,
                             mword const offset, uint8 const size, uint64 &value) override {

        bool ok = false;

        if (access == Vbus::Access::WRITE)
            ok = write(offset, size, value);
        if (access == Vbus::Access::READ)
            ok = read(offset, size, value);

        return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
    }

    virtual void notify(uint32) = 0;
    virtual void driver_ok() = 0;

public:
    Device(const char *name, uint8 const device_id, const Vbus::Bus &bus,
           Model::Irq_controller &irq_ctlr, void *config_space, uint32 config_size,
           uint16 const irq, uint16 const queue_num, uint32 const device_feature_lower = 0)
        : Vbus::Device(name), _irq_ctlr(&irq_ctlr), _vbus(&bus),
          _config_space(reinterpret_cast<uint64 *>(config_space)), _config_size(config_size),
          _irq(irq), _queue_num_max(queue_num), _device_id(device_id),
          _device_feature_lower(device_feature_lower) {}

    uint64 drv_feature() const { return (uint64(_drv_feature_upper) << 32) | _drv_feature_lower; }
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
