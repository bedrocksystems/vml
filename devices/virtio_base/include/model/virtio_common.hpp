/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/simple_as.hpp>
#include <platform/new.hpp>
#include <platform/types.hpp>
#include <platform/virtqueue.hpp>

namespace Virtio {
    class Callback;
    enum class DeviceStatus : uint32;
    enum class Queues : uint32;
    class QueueState;
    struct QueueData;
    struct DeviceState;
    bool read_register(uint64, uint32, uint32, uint8, uint64, uint64 &);
    template<typename T>
    bool write_register(uint64, uint32, uint32, uint8, uint64, T &);
};

class Virtio::Callback {
public:
    virtual void driver_ok() = 0;
};

enum class Virtio::DeviceStatus : uint32 {
    DEVICE_RESET = 0,
    ACKNOWLEDGE = 1,
    DRIVER = 2,
    FAILED = 128,
    FEATURES_OK = 8,
    DRIVER_OK = 4,
    DEVICE_NEEDS_RESET = 64,
};

enum class Virtio::Queues : uint32 {
    MAX = 3,
};

struct Virtio::QueueData {
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

class Virtio::QueueState {
private:
    QueueData *_data{nullptr};
    Virtio::Queue _virtqueue;
    Virtio::DeviceQueue _device_queue{&_virtqueue, 1};
    bool _constructed{false};

public:
    void construct(QueueData &data, Vbus::Bus const &bus) {
        _data = &data;

        /* don't accept queues with zero elements */
        if (_data->num == 0)
            return destruct();

        void *desc_addr = nullptr;
        desc_addr = Model::SimpleAS::gpa_to_vmm_view(bus, GPA(data.descr()),
                                                     Virtio::Descriptor::size(_data->num));
        if (desc_addr == nullptr)
            return destruct();

        void *avail_addr = nullptr;
        avail_addr = Model::SimpleAS::gpa_to_vmm_view(bus, GPA(data.driver()),
                                                      Virtio::Available::size(_data->num));
        if (avail_addr == nullptr)
            return destruct();

        void *used_addr = nullptr;
        used_addr = Model::SimpleAS::gpa_to_vmm_view(bus, GPA(data.device()),
                                                     Virtio::Used::size(_data->num));
        if (used_addr == nullptr)
            return destruct();

        _virtqueue.descriptor = static_cast<Virtio::Descriptor *>(desc_addr);
        _virtqueue.available = static_cast<Virtio::Available *>(avail_addr);
        _virtqueue.used = static_cast<Virtio::Used *>(used_addr);

        new (&_device_queue) Virtio::DeviceQueue(&_virtqueue, uint16(_data->num));

        _constructed = true;
    }

    void destruct() {
        _data = nullptr;
        _constructed = false;
    }

    bool constructed() const { return _constructed; }

    Virtio::DeviceQueue &device_queue() { return _device_queue; }
};

struct Virtio::DeviceState {
    explicit DeviceState(uint16 const num_max, uint32 vendor, uint32 id, uint32 const feature_lower,
                         void *config, uint32 config_sz)
        : queue_num_max(num_max), vendor_id(vendor), device_id(id),
          device_feature_lower(feature_lower), config_space(static_cast<uint64 *>(config)),
          config_size(config_sz) {}

    QueueData const &selected_queue_data() const { return data[sel_queue]; }
    QueueData &selected_queue_data() { return data[sel_queue]; }

    void construct_selected(Vbus::Bus const &bus) {
        if (!queue[sel_queue].constructed()) {
            queue[sel_queue].construct(selected_queue_data(), bus);
        }
    }

    void destruct_selected() {
        if (queue[sel_queue].constructed()) {
            queue[sel_queue].destruct();
        }
    }

    void reset() {
        for (int i = 0; i < static_cast<uint8>(Virtio::Queues::MAX); i++) {
            queue[i].destruct();
            data[i] = {};
        }

        status = 0;
        irq_status = 0;
        drv_device_sel = 0;
        drv_feature_sel = 0;
        drv_feature_upper = 0;
        drv_feature_lower = 0;
    }

    uint32 get_config_gen() const { return __atomic_load_n(&config_generation, __ATOMIC_SEQ_CST); }
    void update_config_gen() { __atomic_fetch_add(&config_generation, 1, __ATOMIC_SEQ_CST); }

    bool status_changed{false};
    bool construct_queue{false};

    bool notify{false};
    uint32 notify_val{0};

    uint16 const queue_num_max; /* spec says: must be power of 2, max 32768 */
    uint32 const vendor_id;
    uint32 const device_id;
    uint32 const device_feature_lower;

    uint64 *config_space;
    uint32 config_size;

    uint32 sel_queue{0};
    uint32 irq_status{0};
    uint32 status{0};
    uint32 drv_device_sel{0};
    uint32 drv_feature_sel{0};
    uint32 drv_feature_upper{0};
    uint32 drv_feature_lower{0};
    uint32 config_generation{0};

    QueueData data[static_cast<uint8>(Virtio::Queues::MAX)];
    QueueState queue[static_cast<uint8>(Virtio::Queues::MAX)];
};

inline bool
Virtio::read_register(uint64 const offset, uint32 const base_reg, uint32 const base_max,
                      uint8 const bytes, uint64 const value, uint64 &result) {
    if (!bytes || (bytes > 8) || (offset + bytes > base_max + 1)) {
        WARN("Register read failure: off " FMTx64 " - base_reg 0x%u - base_max 0x%u - bytes "

             "0x%u",
             offset, base_reg, base_max, bytes);
        return false;
    }

    uint64 const base = offset - base_reg;
    uint64 const mask = (bytes >= 8) ? (0ULL - 1) : ((1ULL << (bytes * 8)) - 1);
    result = (value >> (base * 8)) & mask;
    return true;
}

template<typename T>
inline bool
Virtio::write_register(uint64 const offset, uint32 const base_reg, uint32 const base_max,
                       uint8 const bytes, uint64 const value, T &result) {
    unsigned constexpr TSIZE = sizeof(T);
    if (!bytes || (bytes > TSIZE) || (offset + bytes > base_max + 1)) {
        WARN("Register write failure: off " FMTx64 " - base_reg 0x%u - base_max 0x%u - bytes "
             "0x%u - tsize 0x%u",
             offset, base_reg, base_max, bytes, TSIZE);
        return false;
    }

    uint64 const base = offset - base_reg;
    uint64 const mask = (bytes >= TSIZE) ? (T(0) - 1) : ((T(1) << (bytes * 8)) - 1);

    result &= (bytes >= TSIZE) ? T(0) : ~(T(mask) << (base * 8));
    result |= T(value & mask) << (base * 8);
    return true;
}
