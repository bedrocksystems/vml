/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/irq_controller.hpp>
#include <model/simple_as.hpp>
#include <model/virtqueue.hpp>
#include <platform/atomic.hpp>
#include <platform/bits.hpp>
#include <platform/new.hpp>
#include <platform/string.hpp>
#include <platform/types.hpp>

namespace Virtio {
    class Callback;
    enum class DeviceID : uint16;
    class Transport;
    enum class DeviceStatus : uint32;
    enum class Queues : uint32;
    class QueueState;
    struct QueueData;
    struct DeviceState;
    bool read_register(uint64, uint32, uint32, uint8, uint64, uint64 &);
    template<typename T>
    bool write_register(uint64, uint32, uint32, uint8, uint64, T &);

    class DescrAccessor;
};

class Virtio::Callback {
public:
    virtual void driver_ok() = 0;
};

enum class Virtio::DeviceID : uint16 {
    PLACE_HOLDER = 0,
    NET = 1,
    BLOCK = 2,
    CONSOLE = 3,
    ENTROPY = 4,
    BALLOON = 5,
    SCSI = 8,
    GPU = 16,
    INPUT = 18,
    SOCKET = 19,
    CRYPTO = 20,
    IOMMU = 23,
};

class Virtio::Transport {
public:
    virtual ~Transport() {}

    virtual bool access(Vbus::Access access, mword offset, uint8 size, uint64 &value,
                        Virtio::DeviceState &state)
        = 0;

    virtual void assert_queue_interrupt(Model::Irq_controller *, uint16, Virtio::DeviceState &) = 0;
    virtual void deassert_queue_interrupt(Model::Irq_controller *, uint16, Virtio::DeviceState &)
        = 0;

    virtual void assert_config_change_interrupt(Model::Irq_controller *, uint16,
                                                Virtio::DeviceState &)
        = 0;
    virtual void deassert_config_change_interrupt(Model::Irq_controller *, uint16,
                                                  Virtio::DeviceState &)
        = 0;
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
    QueueData() {}
    explicit QueueData(uint32 n) : num(n) {}

    uint32 descr_low{0};
    uint32 descr_high{0};
    uint32 driver_low{0};
    uint32 driver_high{0};
    uint32 device_low{0};
    uint32 device_high{0};
    uint32 num{0};
    uint32 ready{0};

    // PCI
    uint32 msix_vector{0};
    uint32 notify_off{0};

    uint64 descr() const { return combine_low_high(descr_low, descr_high); }
    uint64 driver() const { return combine_low_high(driver_low, driver_high); }
    uint64 device() const { return combine_low_high(device_low, device_high); }
};

class Virtio::QueueState {
private:
    QueueData *_data{nullptr};
    Virtio::DeviceQueue _device_queue{Virtio::DeviceQueue()};
    bool _constructed{false};

    void *_desc_addr{nullptr};
    void *_avail_addr{nullptr};
    void *_used_addr{nullptr};

public:
    void construct(QueueData &data, Vbus::Bus const &bus) {
        _data = &data;

        /* don't accept queues with zero elements */
        if (_data->num == 0)
            return destruct();

        _desc_addr = Model::SimpleAS::map_guest_mem(
            bus, GPA(data.descr()),
            Virtio::Descriptor::region_size_bytes(static_cast<uint16>(_data->num)), true);
        if (_desc_addr == nullptr)
            return destruct();

        _avail_addr = Model::SimpleAS::map_guest_mem(
            bus, GPA(data.driver()),
            Virtio::Available::region_size_bytes(static_cast<uint16>(_data->num)), true);
        if (_avail_addr == nullptr)
            return destruct();

        _used_addr = Model::SimpleAS::map_guest_mem(
            bus, GPA(data.device()),
            Virtio::Used::region_size_bytes(static_cast<uint16>(_data->num)), true);
        if (_used_addr == nullptr)
            return destruct();

        _device_queue = cxx::move(Virtio::DeviceQueue(_desc_addr, _avail_addr, _used_addr,
                                                      static_cast<uint16>(_data->num)));

        _constructed = true;
    }

    void destruct() {

        _device_queue = cxx::move(Virtio::DeviceQueue());

        if (_desc_addr) {
            Model::SimpleAS::unmap_guest_mem(
                _desc_addr, Virtio::Descriptor::region_size_bytes(static_cast<uint16>(_data->num)));
            _desc_addr = nullptr;
        }
        if (_avail_addr) {
            Model::SimpleAS::unmap_guest_mem(
                _avail_addr, Virtio::Available::region_size_bytes(static_cast<uint16>(_data->num)));
            _avail_addr = nullptr;
        }
        if (_used_addr) {
            Model::SimpleAS::unmap_guest_mem(
                _used_addr, Virtio::Used::region_size_bytes(static_cast<uint16>(_data->num)));
            _used_addr = nullptr;
        }

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
          config_size(config_sz) {
        for (uint16 i = 0; i < static_cast<uint8>(Virtio::Queues::MAX); i++) {
            data[i] = QueueData(queue_num_max);
        }
    }

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
            data[i] = QueueData(queue_num_max);
        }

        status = 0;
        irq_status = 0;
        drv_device_sel = 0;
        drv_feature_sel = 0;
        drv_feature_upper = 0;
        drv_feature_lower = 0;

        memset(tbl_data, 0, sizeof(tbl_data));
        memset(pba_data, 0, sizeof(pba_data));
    }

    uint32 get_config_gen() const { return __atomic_load_n(&config_generation, __ATOMIC_SEQ_CST); }
    void update_config_gen() { __atomic_fetch_add(&config_generation, 1, __ATOMIC_SEQ_CST); }

    bool status_changed{false};
    bool construct_queue{false};
    bool irq_acknowledged{false};

    bool notify{false};
    uint32 notify_val{0};

    uint16 const queue_num_max; /* spec says: must be power of 2, max 32768 */
    uint32 const vendor_id;
    uint32 const device_id;
    uint32 const device_feature_lower;

    uint64 *config_space;
    uint32 config_size;

    uint32 sel_queue{0};
    atomic<uint32> irq_status{0};
    uint32 status{0};
    uint32 drv_device_sel{0};
    uint32 drv_feature_sel{0};
    uint32 drv_feature_upper{0};
    uint32 drv_feature_lower{0};
    uint32 config_generation{0};

    // PCI
    // MSIx config vector for device. The value is itself 16 bit.
    uint32 config_msix_vector{0};

    struct PCIMSIXTBL {
        uint64 msg_addr;
        uint32 msg_data;
        uint32 vec_ctrl;
        PCIMSIXTBL() : msg_addr(0), msg_data(0), vec_ctrl(0) {}
    };

    struct PCIMSIXPBA {
        uint64 bits;
        PCIMSIXPBA() : bits(0) {}
    };

    PCIMSIXTBL tbl_data[64];
    PCIMSIXPBA pba_data[64];

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
    uint64 const mask = (bytes >= TSIZE) ? -1ull : ((1ull << (bytes * 8)) - 1u);

    result &= static_cast<T>((bytes >= TSIZE) ? 0u : ~(mask << (base * 8)));
    result |= static_cast<T>((value & mask) << (base * 8));
    return true;
}
