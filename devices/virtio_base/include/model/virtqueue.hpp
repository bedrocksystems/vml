/**
 * Copyright (c) 2019-2022 BedRock Systems, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <arch/barrier.hpp>
#include <model/foreign_ptr.hpp>
#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/types.hpp>

template<typename T>
inline T
get_offset(const ForeignPtr &fp, size_t offset) {
    return /*Little_endian<T>*/ static_cast<T>(*(fp + offset));
}

template<typename T>
inline void
set_offset(const ForeignPtr &fp, size_t offset, T t) {
    *(fp + offset) = /*Little_endian<T>*/ t;
}

namespace Virtio {
    // [Virtio::Descriptor]s are only created/destroyed by the [Virtio::DeviceQueue]
    // and [Virtio::DriverQueue] - when invoking [recv] and [send], respectively.
    // Some clients - namely the [Virtio::Sg::Buffer] - will manipulate descriptors
    // using a limited API, but most clients care only care about the (linear)
    // *buffer* constituted by a chain of such descriptors and thus never
    // manipulate [Virtio::Descriptor]s directly.
    class Descriptor;

    // [Virtio::Queue] is the base class of [Virtio::DeviceQueue] and
    // [Virtio::DriverQueue] and it contains common members and functionality.
    class Queue;
    class DeviceQueue;
    class DriverQueue;

    class Available;
    class UsedEntry;
    class Used;
};

// Template specializations for moves
// [Virtio::Available]
template typename cxx::remove_reference<Virtio::Available &>::type &&
cxx::move<Virtio::Available &>(Virtio::Available &t) noexcept;
template typename cxx::remove_reference<Virtio::Available>::type &&cxx::move<Virtio::Available>(Virtio::Available &&t) noexcept;
// [Virtio::Descriptor]
template typename cxx::remove_reference<Virtio::Descriptor &>::type &&
cxx::move<Virtio::Descriptor &>(Virtio::Descriptor &t) noexcept;
template typename cxx::remove_reference<Virtio::Descriptor>::type &&
cxx::move<Virtio::Descriptor>(Virtio::Descriptor &&t) noexcept;
// [Virtio::UsedEntry]
template typename cxx::remove_reference<Virtio::UsedEntry &>::type &&
cxx::move<Virtio::UsedEntry &>(Virtio::UsedEntry &t) noexcept;
template typename cxx::remove_reference<Virtio::UsedEntry>::type &&cxx::move<Virtio::UsedEntry>(Virtio::UsedEntry &&t) noexcept;
// [Virtio::Used]
template typename cxx::remove_reference<Virtio::Used &>::type &&cxx::move<Virtio::Used &>(Virtio::Used &t) noexcept;
template typename cxx::remove_reference<Virtio::Used>::type &&cxx::move<Virtio::Used>(Virtio::Used &&t) noexcept;
// [Virtio::DeviceQueue]
template typename cxx::remove_reference<Virtio::DeviceQueue &>::type &&
cxx::move<Virtio::DeviceQueue &>(Virtio::DeviceQueue &t) noexcept;
template typename cxx::remove_reference<Virtio::DeviceQueue>::type &&
cxx::move<Virtio::DeviceQueue>(Virtio::DeviceQueue &&t) noexcept;
// [Virtio::DriverQueue]
template typename cxx::remove_reference<Virtio::DriverQueue &>::type &&
cxx::move<Virtio::DriverQueue &>(Virtio::DriverQueue &t) noexcept;
template typename cxx::remove_reference<Virtio::DriverQueue>::type &&
cxx::move<Virtio::DriverQueue>(Virtio::DriverQueue &&t) noexcept;

enum VirtqAvail : uint16 {
    VIRTQ_AVAIL_NO_INTERRUPT = 0x1,
};

enum VirtqDesc : uint16 {
    VIRTQ_DESC_CONT_NEXT = 0x1,
    VIRTQ_DESC_WRITE_ONLY = 0x2,
    VIRTQ_DESC_INDIRECT_LIST = 0x4,
};

enum VirtqUsed : uint16 {
    VIRTQ_USED_NO_NOTIFY = 0x1,
};

enum VirtioFeature : uint64 {
    VIRTIO_ANY_LAYOUT = 1ULL << 27,
    VIRTIO_INDIRECT_DESC = 1ULL << 28,
    VIRTIO_EVENT_IDX = 1ULL << 29,
    VIRTIO_VERSION_1 = 1ULL << 32,
    VIRTIO_ACCESS_PLATFORM = 1ULL << 33,
    VIRTIO_RING_PACKED = 1ULL << 34,
    VIRTIO_IN_ORDER = 1ULL << 35,
    VIRTIO_ORDER_PLATFORM = 1ULL << 36,
    VIRTIO_SR_IOV = 1ULL << 37,
    VIRTIO_NOTIFICATION_DATA = 1ULL << 38,
};

/*struct Virtio::Descriptor {
    static constexpr uint32 size(uint32 const max_elements) { return 16 * max_elements; }

    uint64 address; // Buffer guest physical address
    uint32 length;  // Buffer length;
    uint16 flags;   // Chained | write/read | indirect.
    uint16 next;    // Only valid if flags mark this descriptor as chained.
};*/
class Virtio::Descriptor {
    // We only allow [Virtio::Queue]/[Virtio::DeviceQueue]/[Virtio::DriverQueue]s to construct
    // non-dummy [Virtio::Descriptor]s. This ensures that all "real" [Virtio::Descriptor]s are
    // valid by construction.
    friend Virtio::Queue;
    friend Virtio::DeviceQueue;
    friend Virtio::DriverQueue;

public:
    // [Virtio::Descriptor]s are *affine* - meaning that they may not be copied
    // and should not be aliased. Therefore we delete copy operators/constructors.
    //
    // Move assignment/construction is permited since it respects the affinity
    // of the [Virtio::Descriptor].
    Virtio::Descriptor &operator=(const Virtio::Descriptor &) = delete;
    Descriptor(const Virtio::Descriptor &) = delete;

    Virtio::Descriptor &operator=(Virtio::Descriptor &&other) {
        if (this != &other) {
            _p = cxx::move(other._p);
            cxx::swap(_desc_idx, other._desc_idx);
        }
        return *this;
    }
    Descriptor(Virtio::Descriptor &&other) {
        _p = cxx::move(other._p);
        cxx::swap(_desc_idx, other._desc_idx);
    }

    // Dummy [Virtio::Descriptor]s are created to reserve space locally for
    // a [Virtio::Descriptor] created by a [Virtio::DeviceQueue]/[Virtio::DriverQueue]
    // call.
    //
    // NOTE: valid descriptor indices only exist in the range [0, 2^15-1) (when the maximum
    // queue size is used). Therefore, we can use [UINT16_MAX] as a sentinel for the "null"
    // [Descriptor].
    Descriptor() : _p(ForeignPtr()), _desc_idx(UINT16_MAX) {}

private:
    // \pre <virtio queue protocol gives access to descriptor located at [desc_idx]>
    Descriptor(void *desc_base, uint16 desc_idx)
        : _p(ForeignPtr(desc_base) + desc_idx * entry_size_bytes()), _desc_idx(desc_idx) {}

public:
    inline bool is_null() const { return _desc_idx == UINT16_MAX; }
    inline uint16 index() const { return _desc_idx; }
    static constexpr size_t entry_size_bytes() { return ENTRY_SIZE_BYTES; }
    static constexpr size_t region_size_bytes(uint16 num_entries) { return num_entries * entry_size_bytes(); }

    inline uint64 address() const { return get_offset<uint64>(_p, ADDR_OFS); }
    inline uint32 length() const { return get_offset<uint32>(_p, LENGTH_OFS); }
    inline uint16 flags() const { return get_offset<uint16>(_p, FLAGS_OFS); }
    inline uint16 next() const { return get_offset<uint16>(_p, NEXT_OFS); }

    inline void set_address(uint64 addr) const { set_offset<uint64>(_p, ADDR_OFS, addr); }
    inline void set_length(uint32 length) const { set_offset<uint32>(_p, LENGTH_OFS, length); }
    inline void set_flags(uint16 flags) const { set_offset<uint16>(_p, FLAGS_OFS, flags); }
    inline void set_next(uint16 next) const { set_offset<uint16>(_p, NEXT_OFS, next); }

private:
    // non-[const] to enable move assignment/construction.
    ForeignPtr _p;
    uint16 _desc_idx;

    static constexpr size_t ADDR_OFS = 0;
    static constexpr size_t LENGTH_OFS = ADDR_OFS + sizeof(uint64);
    static constexpr size_t FLAGS_OFS = LENGTH_OFS + sizeof(uint32);
    static constexpr size_t NEXT_OFS = FLAGS_OFS + sizeof(uint16);
    static constexpr size_t ENTRY_SIZE_BYTES = NEXT_OFS + sizeof(uint16);
};

// Guest (Driver) writes and host (Device) reads from Virtio::Available
/*struct Virtio::Available {
    static constexpr uint32 size(unsigned const max_elements) { return 4U + 2 * max_elements; }

    uint16 flags;
    uint16 index;
    uint16 ring; // head of available ring array.

    // uint16 used_event;  Placed right after the ring whose size is determined at run time and
    // accessed indirectly using ring pointer
};*/
class Virtio::Available {
    // We only allow [Virtio::Queue]/[Virtio::DeviceQueue]/[Virtio::DriverQueue]s to construct
    // [Virtio::Available] regions since they are implementation details of the
    // [Virtio::DeviceQueue]/[Virtio::DriverQueue].
    friend Virtio::Queue;
    friend Virtio::DeviceQueue;
    friend Virtio::DriverQueue;

public:
    Available() {}

    // [Virtio::Available] regions are *affine* - meaning that they may not be copied
    // and should not be aliased. Therefore we delete copy operators/constructors.
    //
    // Move assignment/construction is permited since it respects the affinity
    // of the [Virtio::Available] region.
    Available &operator=(const Available &) = delete;
    Available(const Available &) = delete;

    Available &operator=(Virtio::Available &&other) {
        if (this != &other) {
            _p = cxx::move(other._p);
            cxx::swap(_size, other._size);
        }
        return *this;
    }
    Available(Available &&other) {
        _p = cxx::move(other._p);
        cxx::swap(_size, other._size);
    };

private:
    // For both:
    // \pre <[p] is the base of a Virtio Queue available region of size [size]>
    Available(void *p, uint16 size) : _p(ForeignPtr(p)), _size(size) {}
    Available(ForeignPtr p, uint16 size) : _p(cxx::move(p)), _size(size) {}

public:
    static constexpr size_t entry_size_bytes() { return ENTRY_SIZE_BYTES; }
    static constexpr size_t region_size_bytes(uint16 num_entries) {
        // Section 2.6 of the Virtio Queue standard dictates that memory must be supplied for the
        // used_event/avail_event fields /regardless/ of whether or not the feature is negotiated.
        // \----------------------------------------------------v
        return RING_OFS + num_entries * entry_size_bytes() + sizeof(uint16);
    }
    size_t region_size_bytes() const { return region_size_bytes(_size); }

    inline uint16 flags() const {
        Barrier::w_before_w();
        return get_offset<uint16>(_p, FLAGS_OFS);
    }
    inline uint16 index() const {
        Barrier::w_before_w();
        return get_offset<uint16>(_p, INDEX_OFS);
    }
    inline uint16 ring(size_t index) const { return get_offset<uint16>(_p, RING_OFS + ENTRY_SIZE_BYTES * index); }
    inline uint16 avail_event() const { return get_offset<uint16>(_p, RING_OFS + ENTRY_SIZE_BYTES * _size); }

    inline void set_flags(uint16 flags) const {
        set_offset<uint16>(_p, FLAGS_OFS, flags);
        Barrier::w_before_w();
    }
    inline void set_index(uint16 index) const {
        set_offset<uint16>(_p, INDEX_OFS, index);
        Barrier::w_before_w();
    }
    inline void set_ring(size_t index, uint16 v) const { set_offset<uint16>(_p, RING_OFS + ENTRY_SIZE_BYTES * index, v); }
    inline void set_avail_event(uint16 v) const { set_offset<uint16>(_p, RING_OFS + ENTRY_SIZE_BYTES * _size, v); }

private:
    // non-[const] to enable move assignment/construction.
    ForeignPtr _p{ForeignPtr()};
    uint16 _size{0};

    static constexpr size_t FLAGS_OFS = 0;
    static constexpr size_t INDEX_OFS = FLAGS_OFS + sizeof(uint16);
    static constexpr size_t RING_OFS = INDEX_OFS + sizeof(uint16);
    static constexpr size_t ENTRY_SIZE_BYTES = sizeof(uint16);
};

// Host (Device) writes and guest (Driver) reads from used *ring*.
/*struct Virtio::Used_entry {
    uint32 id;
    uint32 length;
};*/
class Virtio::UsedEntry {
    // A [Virtio::UsedEntry] can only be constructed via a [Virtio::Used::ring] call. At
    // the moment these are created/destroyed repeatedly within [Virtio::DeviceQueue]; we
    // may want to change this strategy.
    friend Virtio::Used;

public:
    // [Virtio::UsedEntry]s are *affine* - meaning that they may not be copied
    // and should not be aliased. Therefore we delete copy operators/constructors.
    //
    // Move assignment/construction is permited since it respects the affinity
    // of the [Virtio::UsedEntry]s.
    UsedEntry &operator=(const UsedEntry &) = delete;
    UsedEntry(const UsedEntry &) = delete;

    UsedEntry &operator=(UsedEntry &&other) {
        if (this != &other) {
            _p = cxx::move(other._p);
        }
        return *this;
    }
    UsedEntry(UsedEntry &&other) { _p = cxx::move(other._p); }

private:
    // For both:
    // \pre <[p] is the base of a Virtio Queue used entry which the caller controls>
    explicit UsedEntry(void *p) : _p(ForeignPtr(p)) {}
    explicit UsedEntry(ForeignPtr p) : _p(cxx::move(p)) {}

public:
    static constexpr size_t size_bytes() { return SIZE_BYTES; }

    inline uint32 id() const { return get_offset<uint32>(_p, ID_OFS); }
    inline uint32 length() const { return get_offset<uint32>(_p, LENGTH_OFS); }

    inline void set_id(uint32 id) const { set_offset<uint32>(_p, ID_OFS, id); }
    inline void set_length(uint32 length) const { set_offset<uint32>(_p, LENGTH_OFS, length); }

private:
    // non-[const] to enable move assignment/construction.
    ForeignPtr _p;

    static constexpr size_t ID_OFS = 0;
    static constexpr size_t LENGTH_OFS = ID_OFS + sizeof(uint32);
    static constexpr size_t SIZE_BYTES = LENGTH_OFS + sizeof(uint32);
};

// Host (Device) writes and guest (Driver) reads from used.
/*struct Virtio::Used {
    static constexpr uint32 size(uint32 const max_elements) { return 4U + 2 * 4 * max_elements; }

    uint16 flags;
    uint16 index;
    Used_entry ring; // head of used ring array.

    // uint16 avail_event; Placed right after the ring whose size is determined at run time and
    // accessed indirectly using ring pointer
};*/
class Virtio::Used {
    // We only allow [Virtio::Queue]/[Virtio::DeviceQueue]/[Virtio::DriverQueue]s to construct
    // [Virtio::Used] regions since they are implementation details of the
    // [Virtio::DeviceQueue]/[Virtio::DriverQueue].
    friend Virtio::Queue;
    friend Virtio::DeviceQueue;
    friend Virtio::DriverQueue;

public:
    Used() {}

    // [Virtio::Used] regions are *affine* - meaning that they may not be copied
    // and should not be aliased. Therefore we delete copy operators/constructors.
    //
    // Move assignment/construction is permited since it respects the affinity
    // of the [Virtio::Used] region.
    Used &operator=(const Virtio::Used &) = delete;
    Used(const Virtio::Used &) = delete;

    Used &operator=(Virtio::Used &&other) {
        if (this != &other) {
            _p = cxx::move(other._p);
            cxx::swap(_size, other._size);
        }
        return *this;
    }
    Used(Used &&other) {
        _p = cxx::move(other._p);
        cxx::swap(_size, other._size);
    }

private:
    Used(void *p, uint16 size) : _p(ForeignPtr(p)), _size(size) {}
    Used(ForeignPtr p, uint16 size) : _p(cxx::move(p)), _size(size) {}

public:
    static constexpr size_t entry_size_bytes() { return ENTRY_SIZE_BYTES; }
    static constexpr size_t region_size_bytes(uint16 num_entries) {
        // Section 2.6 of the Virtio Queue standard dictates that memory must be supplied for the
        // used_event/avail_event fields /regardless/ of whether or not the feature is negotiated.
        // \---------------------------------------------------------------------------------v
        return RING_OFS + num_entries * entry_size_bytes() + sizeof(uint16);
    }
    size_t region_size_bytes() const { return region_size_bytes(_size); }

    inline uint16 flags() const {
        Barrier::w_before_w();
        return get_offset<uint16>(_p, FLAGS_OFS);
    }
    inline uint16 index() const {
        Barrier::w_before_w();
        return get_offset<uint16>(_p, INDEX_OFS);
    }
    inline Virtio::UsedEntry ring(size_t index) const { return UsedEntry(_p + RING_OFS + ENTRY_SIZE_BYTES * index); }
    inline uint16 avail_event() const { return get_offset<uint16>(_p, RING_OFS + ENTRY_SIZE_BYTES * _size); }

    inline void set_flags(uint16 flags) const {
        set_offset<uint16>(_p, FLAGS_OFS, flags);
        Barrier::w_before_w();
    }
    inline void set_index(uint16 index) const {
        set_offset<uint16>(_p, INDEX_OFS, index);
        Barrier::w_before_w();
    }
    inline void set_ring(size_t index, uint32 id, uint32 length) const { set_ring(ring(index), id, length); }
    static inline void set_ring(Virtio::UsedEntry entry, uint32 id, uint32 length) {
        entry.set_id(id);
        entry.set_length(length);
    }
    inline void set_avail_event(uint16 v) const { set_offset<uint16>(_p, RING_OFS + ENTRY_SIZE_BYTES * _size, v); }

private:
    // non-[const] to enable move assignment/construction.
    ForeignPtr _p{ForeignPtr()};
    uint16 _size{0};

    static constexpr size_t FLAGS_OFS = 0;
    static constexpr size_t INDEX_OFS = FLAGS_OFS + sizeof(uint16);
    static constexpr size_t RING_OFS = INDEX_OFS + sizeof(uint16);
    static constexpr size_t ENTRY_SIZE_BYTES = Virtio::UsedEntry::size_bytes();
};

class Virtio::Queue {
public:
    virtual ~Queue() {}

    Queue() {}
    Queue(void *descriptor_base, void *available_base, void *used_base, uint16 sz)
        : _descriptor_base(descriptor_base), _available_base(available_base), _used_base(used_base),
          _available(Virtio::Available(available_base, sz)), _used(Virtio::Used(used_base, sz)), _size(sz) {
        ASSERT(descriptor_base != nullptr);
        ASSERT(available_base != nullptr);
        ASSERT(used_base != nullptr);
        ASSERT(_size != 0);
        ASSERT(_size <= 32768);
        ASSERT((_size & (_size - 1)) == 0);
    }

    // [Virtio::Queue]s are *affine* - meaning that they may not be copied
    // and should not be aliased. Therefore we delete copy operators/constructors.
    //
    // Move assignment/construction is permited since it respects the affinity
    // of the [Virtio::Queues].
    Queue &operator=(const Queue &) = delete;
    Queue(const Queue &) = delete;

    Queue &operator=(Queue &&other) {
        if (this != &other) {
            cxx::swap(_descriptor_base, other._descriptor_base);
            cxx::swap(_available_base, other._available_base);
            cxx::swap(_used_base, other._used_base);
            _available = cxx::move(other._available);
            _used = cxx::move(other._used);
            cxx::swap(_size, other._size);
            cxx::swap(_idx, other._idx);
            cxx::swap(_prev, other._prev);
            cxx::swap(_driven_idx, other._driven_idx);
        }
        return *this;
    }
    Queue(Queue &&other) {
        cxx::swap(_descriptor_base, other._descriptor_base);
        cxx::swap(_available_base, other._available_base);
        cxx::swap(_used_base, other._used_base);
        _available = cxx::move(other._available);
        _used = cxx::move(other._used);
        cxx::swap(_size, other._size);
        cxx::swap(_idx, other._idx);
        cxx::swap(_prev, other._prev);
        cxx::swap(_driven_idx, other._driven_idx);
    }

    mword descriptor_addr() { return reinterpret_cast<mword>(_descriptor_base); }
    mword available_addr() { return reinterpret_cast<mword>(_available_base); }
    mword used_addr() { return reinterpret_cast<mword>(_used_base); }

    uint16 get_size() const { return _size; }

    Errno next_in_chain(const Virtio::Descriptor &desc, uint16 &flags, bool &next_en, uint16 &next,
                        Virtio::Descriptor &next_desc);

    virtual bool is_device_queue() const = 0;
    inline bool is_driver_queue() const { return not(is_device_queue()); }

    // NOTE: [Virtio::DriverQueue] will ignore the [len] parameter
    virtual void send(Virtio::Descriptor &&desc, uint32 len) = 0;
    virtual Errno recv(Virtio::Descriptor &desc) = 0;

protected:
    inline uint16 count_available(uint16 idx) const {
        // Index is a 16 bit free running counter. The max limit for ring size is 32768. Therefore,
        // a difference in local index copy and the available index set by guest is always
        // indicative of buffers available to be processed.
        return (idx >= _idx) ? idx - _idx : (UINT16_MAX - _idx) + 1 + idx;
    }
    inline uint16 count_free(uint16 idx) const { return static_cast<uint16>(_size - count_available(idx)); }
    inline uint16 used_index() const { return _used.index(); }
    inline uint16 available_index() const { return _available.index(); }

protected:
    void *_descriptor_base{nullptr};
    void *_available_base{nullptr};
    void *_used_base{nullptr};

    // NOTE: [_available_base]/[_used_base] must coincide with address used to construct
    // [_available]/[_used], respectively.
    Virtio::Available _available;
    Virtio::Used _used;

    uint16 _size{0};

    // Local running index for the ring driven by the /other/ party. Count from 0 - 65535 and wraps
    // to zero.
    uint16 _idx{0};

    // Stores previous index value for the ring driven by /this/ party.
    uint16 _prev{0};
    // Stores current index value for the ring driven by /this/ party.
    uint16 _driven_idx{0};
};

class Virtio::DeviceQueue final : public Virtio::Queue {
public:
    ~DeviceQueue() override {}

    DeviceQueue() {}
    DeviceQueue(void *descriptor_base, void *available_base, void *used_base, uint16 sz)
        : Virtio::Queue(descriptor_base, available_base, used_base, sz) {}

    // [Virtio::DeviceQueue]s are *affine* - meaning that they may not be copied
    // and should not be aliased. Therefore we delete copy operators/constructors.
    //
    // Move assignment/construction is permited since it respects the affinity
    // of the [DeviceVirtio::Queues].
    DeviceQueue &operator=(const DeviceQueue &) = delete;
    DeviceQueue(const DeviceQueue &) = delete;

    DeviceQueue &operator=(DeviceQueue &&other) = default;
    DeviceQueue(DeviceQueue &&other) = default;

    void send(Virtio::Descriptor &&desc, uint32 len) override;
    Errno recv(Virtio::Descriptor &desc) override;

    bool is_device_queue() const override { return true; }

    uint16 get_available() const;
    uint16 get_free() const;
    bool used_event_notify() const;
    bool interrupts_disabled() const;
    void enable_notifications();
    void disable_notifications();

private:
    // Device manipulates avail_event field to suggest driver to suppress notifications till it has
    // added avail_event number of buffers to queue.
    inline void set_avail_event(uint16 index) { _used.set_avail_event(index); }

    // Device reads the used_event field to send notifications after consuming used_event number of
    // buffers.
    inline uint16 get_used_event() const { return _available.avail_event(); }
};

class Virtio::DriverQueue final : public Virtio::Queue {
public:
    ~DriverQueue() override {}

    DriverQueue() {}
    DriverQueue(void *descriptor_base, void *available_base, void *used_base, uint16 sz)
        : Virtio::Queue(descriptor_base, available_base, used_base, sz) {}

    // [Virtio::DriverQueue]s are *affine* - meaning that they may not be copied
    // and should not be aliased. Therefore we delete copy operators/constructors.
    //
    // Move assignment/construction is permited since it respects the affinity
    // of the [Virtio::DriverQueues].
    DriverQueue &operator=(const DriverQueue &dev_q) = delete;
    DriverQueue(const DriverQueue &dev_q) = delete;

    DriverQueue &operator=(DriverQueue &&dev_q) = default;
    DriverQueue(DriverQueue &&dev_q) = default;

    // NOTE: The Driver must initially create descriptors, which
    // it then [send]s to (and [recv]s back from) the Device. This should
    // only be invoked once per entry within the virtio queue.
    Virtio::Descriptor initialize_descriptor(uint16 desc_idx) const;

    // NOTE: [Virtio::DriverQueue] will ignore the [len] parameter
    void send(Virtio::Descriptor &&desc, uint32) override;
    Errno recv(Virtio::Descriptor &desc) override;

    bool is_device_queue() const override { return false; }

    uint16 get_available() const;
    uint16 get_free() const;
    bool notifications_disabled();
    void enable_interrupts();
    void disable_interrupts();

    // Helpers to create DriverQueue from regions allocated from heap.
    static Errno create_driver_queue(uint16 num_entries, Virtio::DriverQueue &out);
    static void delete_driver_queue(Virtio::DriverQueue &queue);

private:
    // Driver manipulates used_event field to suggest device to suppress iterrupts till it has
    // added used_event number of buffers to queue.
    inline void set_used_event(uint16 index) { _available.set_avail_event(index); }
};
