/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#pragma once

#include <model/irq_controller.hpp>
#include <platform/new.hpp>
#include <platform/types.hpp>
#include <platform/virtqueue.hpp>

namespace Virtio {
    class Console;
    class Device;
    class Queue_state;
    struct Queue_data;
    class Ram;
    class Callback;
};

class Virtio::Ram {
private:
    uint64 const _base;
    uint64 const _size;
    uint64 const _local;

public:
    Ram(uint64 const addr, uint64 const sz, uint64 const local)
        : _base(addr), _size(sz), _local(local) {}

    uint64 base() const { return _base; }
    uint64 size() const { return _size; }
    uint64 local() const { return _local; }

    bool local_address(uint64 const guest, uint32 const access_size, uint64 &vmm_addr) const {
        if (guest < _base || guest >= _base + _size)
            return false;
        if (_base + _size - guest < access_size)
            return false;

        vmm_addr = _local + (guest - _base);
        return true;
    }
};

class Virtio::Callback {
public:
    virtual void driver_ok() = 0;
};

struct Virtio::Queue_data {
    uint32 descr_low{0};
    uint32 descr_high{0};
    uint32 driver_low{0};
    uint32 driver_high{0};
    uint32 device_low{0};
    uint32 device_high{0};
    uint32 num{0};
    uint32 ready{0};

    uint64 descr() const { return (uint64(descr_high) << 32) | descr_low; }
    uint64 driver() const { return (uint64(driver_high) << 32) | driver_low; }
    uint64 device() const { return (uint64(device_high) << 32) | device_low; }
};

class Virtio::Queue_state {
private:
    Queue_data *_data{nullptr};
    Virtio::Queue _virtqueue;
    Virtio::DeviceQueue _device_queue{nullptr, 0};
    bool _constructed{false};

public:
    void construct(Queue_data &data, Ram const &ram) {
        _data = &data;

        /* don't accept queues with zero elements */
        if (_data->num == 0)
            return destruct();

        uint64 desc_addr = 0;
        if (!ram.local_address(data.descr(), Virtio::Descriptor::size(_data->num), desc_addr))
            return destruct();

        uint64 avail_addr = 0;
        if (!ram.local_address(data.driver(), Virtio::Available::size(_data->num), avail_addr))
            return destruct();

        uint64 used_addr = 0;
        if (!ram.local_address(data.device(), Virtio::Used::size(_data->num), used_addr))
            return destruct();

        _virtqueue.descriptor = reinterpret_cast<Virtio::Descriptor *>(desc_addr);
        _virtqueue.available = reinterpret_cast<Virtio::Available *>(avail_addr);
        _virtqueue.used = reinterpret_cast<Virtio::Used *>(used_addr);

        new (&_device_queue) Virtio::DeviceQueue(&_virtqueue, uint16(_data->num));

        _constructed = true;
    }

    void destruct() {
        _data = nullptr;
        _constructed = false;
    }

    bool constructed() const { return _constructed; }

    Virtio::DeviceQueue &queue() { return _device_queue; }
};

class Virtio::Device {
protected:
    enum { RX = 0, TX = 1, QUEUES = 2 };

    Model::Irq_controller *const _irq_ctlr;
    Ram const *const _ram;
    uint64 *_config_space{nullptr};
    uint32 _config_size;

    uint16 const _irq;
    uint16 const _queue_num_max; /* spec says: must be power of 2, max 32768 */
    uint8 const _device_id;
    uint32 const _device_feature_lower;

    uint32 _sel_queue{RX};
    uint32 _vendor_id{0x554d4551ULL /* QEMU */};
    uint32 _irq_status{0};
    uint32 _status{0};
    uint32 _drv_device_sel{0};
    uint32 _drv_feature_sel{0};
    uint32 _drv_feature_upper{0};
    uint32 _drv_feature_lower{0};
    uint32 _config_generation{0};

    Queue_data _data[QUEUES];
    Queue_state _queue[QUEUES];

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

    enum Device_status {
        DEVICE_RESET = 0,
        ACKNOWLEDGE = 1,
        DRIVER = 2,
        FAILED = 128,
        FEATURES_OK = 8,
        DRIVER_OK = 4,
        DEVICE_NEEDS_RESET = 64,
    };

    Queue_data const &_queue_data() const { return _data[_sel_queue]; }
    Queue_data &_queue_data() { return _data[_sel_queue]; }

    void _queue_state(bool const construct) {
        if (construct && !_queue[_sel_queue].constructed()) {
            _queue[_sel_queue].construct(_queue_data(), *_ram);
        }

        if (!construct && _queue[_sel_queue].constructed()) {
            _queue[_sel_queue].destruct();
        }
    }

    void _reset() {
        _queue[RX].destruct();
        _queue[TX].destruct();
        _data[RX] = {};
        _data[TX] = {};

        _status = 0;
        _irq_status = 0;
        _drv_device_sel = 0;
        _drv_feature_sel = 0;
        _drv_feature_upper = 0;
        _drv_feature_lower = 0;
    }

    void _assert_irq() {
        _irq_status = 0x1;
        _irq_ctlr->assert_spi(_irq);
    }

    void _deassert_irq() { _irq_status = 0; }

    void _update_config_gen() { __atomic_fetch_add(&_config_generation, 1, __ATOMIC_SEQ_CST); }

    bool _read(uint64 const offset, uint8 const bytes, uint64 &value) const {
        if (bytes > 4)
            return false;

        switch (offset) {
        case RO_MAGIC ... RO_MAGIC_END:
            return _read_register(offset, RO_MAGIC, RO_MAGIC_END, bytes, 0x74726976ULL, value);
        case RO_VERSION ... RO_VERSION_END:
            return _read_register(offset, RO_VERSION, RO_VERSION_END, bytes, 2, value);
        case RO_DEVICE_ID ... RO_DEVICE_ID_END:
            return _read_register(offset, RO_DEVICE_ID, RO_DEVICE_ID_END, bytes, _device_id, value);
        case RO_VENDOR_ID ... RO_VENDOR_ID_END:
            return _read_register(offset, RO_VENDOR_ID, RO_VENDOR_ID_END, bytes, _vendor_id, value);
        case RO_DEVICE_FEATURE ... RO_DEVICE_FEATURE_END:
            if (_drv_device_sel == 0)
                return _read_register(offset, RO_DEVICE_FEATURE, RO_DEVICE_FEATURE_END, bytes,
                                      _device_feature_lower, value);
            else
                return _read_register(offset, RO_DEVICE_FEATURE, RO_DEVICE_FEATURE_END, bytes,
                                      1 /*VIRTIO_F_VERSION_1*/, value);
        case RW_DEVICE_FEATURE_SEL ... RW_DEVICE_FEATURE_SEL_END:
            return _read_register(offset, RW_DEVICE_FEATURE_SEL, RW_DEVICE_FEATURE_SEL_END, bytes,
                                  _drv_device_sel, value);
        case RW_DRIVER_FEATURE_SEL ... RW_DRIVER_FEATURE_SEL_END:
            return _read_register(offset, RW_DRIVER_FEATURE_SEL, RW_DRIVER_FEATURE_SEL_END, bytes,
                                  _drv_feature_sel, value);
        case RO_QUEUE_NUM_MAX ... RO_QUEUE_NUM_MAX_END:
            return _read_register(offset, RO_QUEUE_NUM_MAX, RO_QUEUE_NUM_MAX_END, bytes,
                                  _queue_num_max, value);
        case RW_QUEUE_READY ... RW_QUEUE_READY_END:
            return _read_register(offset, RW_QUEUE_READY, RW_QUEUE_READY_END, bytes,
                                  _queue_data().ready, value);
        case RO_IRQ_STATUS ... RO_IRQ_STATUS_END:
            return _read_register(offset, RO_IRQ_STATUS, RO_IRQ_STATUS_END, bytes, _irq_status,
                                  value);
        case RW_STATUS ... RW_STATUS_END:
            return _read_register(offset, RW_STATUS, RW_STATUS_END, bytes, _status, value);

        case RO_CONFIG_GENERATION ... RO_CONFIG_GENERATION_END:
            return _read_register(offset, RO_CONFIG_GENERATION, RO_CONFIG_GENERATION_END, bytes,
                                  __atomic_load_n(&_config_generation, __ATOMIC_SEQ_CST), value);

        // Config space access can be byte aligned.
        case RW_CONFIG ... RW_CONFIG_END:
            return _read_register(offset, RW_CONFIG, (RW_CONFIG + _config_size - 1), bytes,
                                  _config_space[(offset & ~(8ULL - 1)) - RW_CONFIG], value);
        }
        return false;
    }

    bool _write(uint64 const offset, uint8 const bytes, uint64 const value) {
        if (bytes > 4)
            return false;

        switch (offset) {
        case RW_DEVICE_FEATURE_SEL ... RW_DEVICE_FEATURE_SEL_END:
            return _write_register(offset, RW_DEVICE_FEATURE_SEL, RW_DEVICE_FEATURE_SEL_END, bytes,
                                   value, _drv_device_sel);
        case WO_DRIVER_FEATURE ... WO_DRIVER_FEATURE_END:
            if (_drv_feature_sel == 0)
                return _write_register(offset, WO_DRIVER_FEATURE, WO_DRIVER_FEATURE_END, bytes,
                                       value, _drv_feature_lower);
            else
                return _write_register(offset, WO_DRIVER_FEATURE, WO_DRIVER_FEATURE_END, bytes,
                                       value, _drv_feature_upper);
        case RW_DRIVER_FEATURE_SEL ... RW_DRIVER_FEATURE_SEL_END:
            return _write_register(offset, RW_DRIVER_FEATURE_SEL, RW_DRIVER_FEATURE_SEL_END, bytes,
                                   value, _drv_feature_sel);
        case WO_QUEUE_SEL ... WO_QUEUE_SEL_END:
            if (value >= QUEUES)
                return true; /* ignore out of bound */
            return _write_register(offset, WO_QUEUE_SEL, WO_QUEUE_SEL_END, bytes, value,
                                   _sel_queue);
        case WO_QUEUE_NUM ... WO_QUEUE_NUM_END:
            if (value > _queue_num_max)
                return true; /* ignore out of bound */
            return _write_register(offset, WO_QUEUE_NUM, WO_QUEUE_NUM_END, bytes, value,
                                   _queue_data().num);
        case RW_QUEUE_READY ... RW_QUEUE_READY_END: {
            if (!_write_register(offset, RW_QUEUE_READY, RW_QUEUE_READY_END, bytes, value,
                                 _queue_data().ready))
                return false;
            bool const construct = (value == 1) ? true : false;
            _queue_state(construct);
            return true;
        }
        case WO_IRQ_ACK ... WO_IRQ_ACK_END:
            return true;
        case RW_STATUS ... RW_STATUS_END:
            if (value == DEVICE_RESET)
                _reset();
            else if (value & DRIVER_OK)
                _driver_ok();
            return _write_register(offset, RW_STATUS, RW_STATUS_END, bytes, value, _status);
        case WO_QUEUE_NOTIFY:
            _notify(uint32(value));
            return true;
        case WO_QUEUE_DESCR_LOW ... WO_QUEUE_DESCR_LOW_END:
            return _write_register(offset, WO_QUEUE_DESCR_LOW, WO_QUEUE_DESCR_LOW_END, bytes, value,
                                   _queue_data().descr_low);
        case WO_QUEUE_DESCR_HIGH ... WO_QUEUE_DESCR_HIGH_END:
            return _write_register(offset, WO_QUEUE_DESCR_HIGH, WO_QUEUE_DESCR_HIGH_END, bytes,
                                   value, _queue_data().descr_high);
        case WO_QUEUE_DRIVER_LOW ... WO_QUEUE_DRIVER_LOW_END:
            return _write_register(offset, WO_QUEUE_DRIVER_LOW, WO_QUEUE_DRIVER_LOW_END, bytes,
                                   value, _queue_data().driver_low);
        case WO_QUEUE_DRIVER_HIGH ... WO_QUEUE_DRIVER_HIGH_END:
            return _write_register(offset, WO_QUEUE_DRIVER_HIGH, WO_QUEUE_DRIVER_HIGH_END, bytes,
                                   value, _queue_data().driver_high);
        case WO_QUEUE_DEVICE_LOW ... WO_QUEUE_DEVICE_LOW_END:
            return _write_register(offset, WO_QUEUE_DEVICE_LOW, WO_QUEUE_DEVICE_LOW_END, bytes,
                                   value, _queue_data().device_low);
        case WO_QUEUE_DEVICE_HIGH ... WO_QUEUE_DEVICE_HIGH_END:
            return _write_register(offset, WO_QUEUE_DEVICE_HIGH, WO_QUEUE_DEVICE_HIGH_END, bytes,
                                   value, _queue_data().device_high);
        // Config space access can be byte aligned.
        case RW_CONFIG ... RW_CONFIG_END:
            return _write_register(offset, RW_CONFIG, (RW_CONFIG + _config_size - 1), bytes, value,
                                   _config_space[(offset & ~(8ULL - 1)) - RW_CONFIG]);
        }
        return false;
    }

    bool _read_register(uint64 const offset, uint32 const base_reg, uint32 const base_max,
                        uint8 const bytes, uint64 const value, uint64 &result) const {
        if (!bytes || (bytes > 8) || (offset + bytes > base_max + 1))
            return false;

        uint64 const base = offset - base_reg;
        uint64 const mask = (bytes >= 8) ? (0ULL - 1) : ((1ULL << (bytes * 8)) - 1);
        result = (value >> (base * 8)) & mask;
        return true;
    }

    template<typename T>
    bool _write_register(uint64 const offset, uint32 const base_reg, uint32 const base_max,
                         uint8 const bytes, uint64 const value, T &result) {
        unsigned constexpr tsize = sizeof(T);
        if (!bytes || (bytes > tsize) || (offset + bytes > base_max + 1))
            return false;

        uint64 const base = offset - base_reg;
        uint64 const mask = (bytes >= tsize) ? (T(0) - 1) : ((T(1) << (bytes * 8)) - 1);

        result &= (bytes >= tsize) ? T(0) : ~(T(mask) << (base * 8));
        result |= T(value & mask) << (base * 8);
        return true;
    }

    virtual void _notify(uint32) = 0;
    virtual void _driver_ok() = 0;

public:
    Device(uint8 const device_id, Ram const &ram, Model::Irq_controller &irq_ctlr,
           void *config_space, uint32 config_size, uint16 const irq, uint16 const queue_num,
           uint32 const device_feature_lower = 0)
        : _irq_ctlr(&irq_ctlr), _ram(&ram), _config_space(reinterpret_cast<uint64 *>(config_space)),
          _config_size(config_size), _irq(irq), _queue_num_max(queue_num), _device_id(device_id),
          _device_feature_lower(device_feature_lower) {}

    uint64 drv_feature() const { return (uint64(_drv_feature_upper) << 32) | _drv_feature_lower; }
};
