/**
 * Copyright (C) 2021-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#pragma once

#include <model/iommu_interface.hpp>
#include <model/irq_controller.hpp>
#include <model/simple_as.hpp>
#include <model/virtqueue.hpp>
#include <platform/atomic.hpp>
#include <platform/bits.hpp>
#include <platform/log.hpp>
#include <platform/string.hpp>
#include <platform/type_traits.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Virtio {
    class Callback;
    enum class DeviceID : uint16;
    class Transport;
    enum class DeviceStatus : uint32;
    enum class Queues : uint32;
    enum FeatureBits : uint64;
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

// These are device-independent feature bits as per VirtIO specs section [6 Reserved Feature Bits]
enum Virtio::FeatureBits : uint64 {
    VIRTIO_F_INDIRECT_DESC = 1ULL << 28,
    VIRTIO_F_EVENT_IDX = 1ULL << 29,
    VIRTIO_F_VERSION_1 = 1ULL << 32,
    VIRTIO_F_ACCESS_PLATFORM = 1ULL << 33,
    VIRTIO_F_RING_PACKED = 1ULL << 34,
    VIRTIO_F_IN_ORDER = 1ULL << 35,
    VIRTIO_F_ORDER_PLATFORM = 1ULL << 36,
    VIRTIO_F_SR_IOV = 1ULL << 37,
    VIRTIO_F_NOTIFICATION_DATA = 1ULL << 38,
    VIRTIO_F_NOTIF_CONFIG_DATA = 1ULL << 39,
    VIRTIO_F_RING_RESET = 1ULL << 40,
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
    uint16 _queue_sz_num_descs{0};
    QueueData *_data{nullptr};
    Virtio::DeviceQueue _device_queue{Virtio::DeviceQueue()};
    bool _constructed{false};

    void *_desc_addr{nullptr};
    void *_avail_addr{nullptr};
    void *_used_addr{nullptr};

private:
    static GPA convert(Model::IOMMUManagedDevice &io_translations, uint64 addr, size_t size_bytes, bool use_io_translation) {
        if (not use_io_translation) {
            return GPA(addr);
        }

        return GPA(io_translations.translate_io(addr, size_bytes));
    }

public:
    void construct(QueueData &queue_data, Vbus::Bus const &bus, bool use_io_translation,
                   Model::IOMMUManagedDevice &io_translations) {
        if (not Virtio::Queue::is_size_valid(static_cast<uint16>(queue_data.num)))
            return;

        _data = &queue_data;
        _queue_sz_num_descs = static_cast<uint16>(_data->num);

        GPA data = convert(io_translations, queue_data.descr(), Virtio::Descriptor::region_size_bytes(_queue_sz_num_descs),
                           use_io_translation);
        GPA driver = convert(io_translations, queue_data.driver(), Virtio::Available::region_size_bytes(_queue_sz_num_descs),
                             use_io_translation);
        GPA device = convert(io_translations, queue_data.device(), Virtio::Used::region_size_bytes(_queue_sz_num_descs),
                             use_io_translation);

        if (data.invalid() or driver.invalid() or device.invalid()) {
            return destruct();
        }

        _desc_addr = Model::SimpleAS::map_guest_mem(bus, data, Virtio::Descriptor::region_size_bytes(_queue_sz_num_descs), true);
        if (_desc_addr == nullptr)
            return destruct();

        _avail_addr
            = Model::SimpleAS::map_guest_mem(bus, driver, Virtio::Available::region_size_bytes(_queue_sz_num_descs), true);

        if (_avail_addr == nullptr)
            return destruct();

        _used_addr = Model::SimpleAS::map_guest_mem(bus, device, Virtio::Used::region_size_bytes(_queue_sz_num_descs), true);
        if (_used_addr == nullptr)
            return destruct();

        _device_queue = cxx::move(Virtio::DeviceQueue(_desc_addr, _avail_addr, _used_addr, _queue_sz_num_descs));

        _constructed = true;
    }

    void destruct() {
        _device_queue = cxx::move(Virtio::DeviceQueue());

        if (_desc_addr != nullptr) {
            Model::SimpleAS::unmap_guest_mem(_desc_addr, Virtio::Descriptor::region_size_bytes(_queue_sz_num_descs));
            _desc_addr = nullptr;
        }
        if (_avail_addr != nullptr) {
            Model::SimpleAS::unmap_guest_mem(_avail_addr, Virtio::Available::region_size_bytes(_queue_sz_num_descs));
            _avail_addr = nullptr;
        }
        if (_used_addr != nullptr) {
            Model::SimpleAS::unmap_guest_mem(_used_addr, Virtio::Used::region_size_bytes(_queue_sz_num_descs));
            _used_addr = nullptr;
        }

        _queue_sz_num_descs = 0;
        _data = nullptr;
        _constructed = false;
    }

    bool constructed() const { return _constructed; }

    Virtio::DeviceQueue &device_queue() { return _device_queue; }
};

struct Virtio::DeviceState {
    explicit DeviceState(uint16 const num_max, uint32 vendor, uint32 id, uint64 const feature, void *config, uint32 config_sz)
        : queue_num_max(num_max), vendor_id(vendor), device_id(id), device_feature_lower(static_cast<uint32>(feature)),
          // We always set [VIRTIO_F_VERSION_1] i.e. no legacy VirtIO emulation.
          device_feature_upper(static_cast<uint32>((feature | Virtio::FeatureBits::VIRTIO_F_VERSION_1) >> 32)),
          config_space(static_cast<uint8 *>(config)), config_size(config_sz) {
        for (uint16 i = 0; i < static_cast<uint8>(Virtio::Queues::MAX); i++) {
            data[i] = QueueData(queue_num_max);
        }
    }

    QueueData const &selected_queue_data() const { return data[sel_queue]; }
    QueueData &selected_queue_data() { return data[sel_queue]; }

    void construct_selected(Vbus::Bus const &bus, bool use_io_translation, Model::IOMMUManagedDevice &io_translations) {
        if (!queue[sel_queue].constructed()) {
            queue[sel_queue].construct(selected_queue_data(), bus, use_io_translation, io_translations);
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

    bool is_driver_ok_state() const { return status == static_cast<uint32>(Virtio::DeviceStatus::DRIVER_OK); }

    uint32 get_config_gen() const { return __atomic_load_n(&config_generation, __ATOMIC_SEQ_CST); }
    void update_config_gen() { __atomic_fetch_add(&config_generation, 1, __ATOMIC_SEQ_CST); }

    bool platform_specific_access_enabled() const {
        // [device_feature_upper == device(host) offered the feature bit
        // [drv_feature_upper] == driver(guest) negotiated the feature
        return 0
               != (device_feature_upper & drv_feature_upper
                   & static_cast<uint32>(Virtio::FeatureBits::VIRTIO_F_ACCESS_PLATFORM >> 32));
    }

    bool status_changed{false};
    bool construct_queue{false};
    bool irq_acknowledged{false};

    bool notify{false};
    uint32 notify_val{0};

    uint16 const queue_num_max; /* spec says: must be power of 2, max 32768 */
    uint32 const vendor_id;
    uint32 const device_id;
    uint32 const device_feature_lower;
    uint32 const device_feature_upper;

    uint8 *config_space;
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
        uint64 msg_addr{0};
        uint32 msg_data{0};
        uint32 vec_ctrl{0};
        PCIMSIXTBL() = default;
    };

    struct PCIMSIXPBA {
        uint64 bits{0};
        PCIMSIXPBA() = default;
    };

    PCIMSIXTBL tbl_data[64];
    PCIMSIXPBA pba_data[64];

    static constexpr size_t MSIX_TBL_SIZE = sizeof(tbl_data);
    static constexpr size_t MSIX_PBA_SIZE = sizeof(pba_data);

    QueueData data[static_cast<uint8>(Virtio::Queues::MAX)];
    QueueState queue[static_cast<uint8>(Virtio::Queues::MAX)];
};

inline bool
Virtio::read_register(uint64 const offset, uint32 const base_reg, uint32 const base_max, uint8 const bytes, uint64 const value,
                      uint64 &result) {
    if ((bytes == 0u) || (bytes > 8) || (offset + bytes > base_max + 1)) {
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
Virtio::write_register(uint64 const offset, uint32 const base_reg, uint32 const base_max, uint8 const bytes, uint64 const value,
                       T &result) {
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

class Virtio::Transport {
public:
    virtual ~Transport() {}

    virtual bool access(Vbus::Access access, mword offset, uint8 size, uint64 &value, Virtio::DeviceState &state) = 0;

    virtual void assert_queue_interrupt(Model::IrqController *, uint16, Virtio::DeviceState &) = 0;
    virtual void deassert_queue_interrupt(Model::IrqController *, uint16, Virtio::DeviceState &) = 0;

    static bool config_space_read(uint64 const offset, uint64 const config_base, uint8 const bytes, uint64 &value,
                                  const Virtio::DeviceState &state) {
        uint64 off_in_config = (offset - config_base);
        if (off_in_config + bytes > state.config_size)
            return false;

        memcpy(&value, state.config_space + off_in_config, bytes);
        return true;
    }

    static bool config_space_write(uint64 const offset, uint64 const config_base, uint8 const bytes, uint64 const value,
                                   Virtio::DeviceState &state) {
        uint64 off_in_config = (offset - config_base);
        if (off_in_config + bytes > state.config_size)
            return false;

        memcpy(state.config_space + off_in_config, &value, bytes);
        return true;
    }
};
