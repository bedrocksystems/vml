/*
 * Copyright (c) 2019-2022 BedRock Systems, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/virtqueue.hpp>
#include <platform/bits.hpp>
#include <platform/new.hpp>
#include <platform/string.hpp>

/** [Virtio::Queue] */
namespace Virtio {
    // [flags] and [next] must be read from shared memory during this operation, so we return their
    // values in the [flags]/[next] reference arguments to encourage clients to avoid double-reading
    // the shared memory.
    Errno Queue::next_in_chain(const Descriptor &desc, uint16 &flags, bool &next_en, uint16 &next, Descriptor &next_desc) {
        flags = desc.flags();

        if ((flags & VIRTQ_DESC_CONT_NEXT) != 0) {
            next_en = true;
            next = desc.next();

            // Through malice or accident, an invalid descriptor index has been used within a chain;
            // the best we can do is avoid corrupting guest memory.
            if (next >= _size)
                return Errno::NOTRECOVERABLE;

            next_desc = Descriptor(_descriptor_base, next);
        } else {
            next_en = false;
        }

        return Errno::NONE;
    }
};

/** [Virtio::DeviceQueue] */
namespace Virtio {
    // Send a descriptor chain back to guest with the actual buffer size consumed by host.
    // [sz] is a /lowerbound/ on the number of bytes written into the prefix of the wrtiable
    // portion of a descriptor chain.
    //
    // Virtio specs: 2.6.8 The Virtqueue Used Ring:
    // struct virtq_used_elem {
    //      /* Index of start of used descriptor chain. */
    //      le32 id;
    //      /* Total length of the descriptor chain which was used (written to) */
    //      le32 len;
    // };
    //  "len is particularly useful for drivers using untrusted buffers: if a driver does
    //  not know exactly how much has been written by the device, the driver would have
    //  to zero the buffer in advance to ensure no data leakage occurs."
    void DeviceQueue::send(Descriptor &&desc, uint32 len) {
        // NOTE: The virtio queue standard talks about the use of memory barrier by drivers
        // when sending chains of descriptors to devices, but it does not talk explicitly about
        // the other direction. We choose to mirror the use of barriers for the device side.
        //
        // cf. 2.6.13.1 (precondition) - Driver sets up the buffer in the descriptor table,
        //                               and [desc] is the root of that chain

        // We store the used index in which we're placing [desc] to support the used_event_notify
        // feature which requires comparing current and previous used index values.
        //
        // cf. 2.6.12.2 - Driver places the index of the head of the descriptor chain into the
        //                next ring entry of the available ring.
        _prev = _driven_idx++;
        _used.set_ring(_prev % _size, desc.index(), len);

        // cf. 2.6.13.3 - Batching is allowed, but we don't do it

        // cf. 2.6.13.4 - The driver performs a suitable memory barrier to ensure the device sees
        //                the updated descriptor table and available ring before the next step.
        //
        // We're doing the dual of this.
        Barrier::w_before_w();

        // Increment the used index.
        //
        // cf. 2.6.13.5 - The available index is increased by the number of descriptor chain heads
        //                added to the available ring.
        // cf. 2.6.13.6 - The driver performs a suitable memory barrier to ensure that it updates
        // ^              the idx field before checking for notification suppression.
        //  \---- NOTE: the implementation of this function inserts the appropriate synchronization.
        _used.set_index(_driven_idx);

        // cf. 2.6.13.7 - The driver send an available buffer notification if such notifications
        //                are not suppressed.
    }

    // "Receive" the head of a descriptor chain from the guest. It retrieves the head of a chain of
    // descriptors to be processed and modified by the host. If the return value is [Errno::NONE]
    // then the reference argument will contain the head of a descriptor chain, else it will be left
    // unchanged.
    //
    // NOTE: In virtio, a chain of descriptors is considered a single buffer. [available->index] and
    // [used->index]  are incremented once for each buffer.
    Errno DeviceQueue::recv(Descriptor &desc) {
        // NOTE: The virtio queue standard talks about the use of memory barrier by drivers
        // when sending chains of descriptors to devices, but it does not talk explicitly about
        // the other direction. We choose to mirror the use of barriers for the device side.

        // NOTE: the implementation of this function inserts the appropriate synchronization.
        uint16 avail_idx = available_index();

        // NOTE: make sure to test whether or not there are any descriptors available
        // prior to modifying the shared memory in any way.
        if (count_available(avail_idx) == 0)
            return Errno::NOENT;

        // To support interrupt/notification suppression features.
        // If VIRTIO_EVENT_IDX is negotiated, we want to receive a notification from guest when it
        // makes new buffers available.
        set_avail_event(avail_idx);

        // The index retrieved from available ring is the head of descriptor chain which needs to
        // be provided to used ring while marking a descriptor as used. Caller cannot manage it by
        // using it as a counter because the guest may use the same index again if it was reclaimed
        // before next transfer.
        auto available_ring_idx = _available.ring(_idx++ % _size);

        // It can happen due to a buggy guest implementation and/or an attack.
        // In any case, the virtio spec is violated and there is no gurantee of recovering
        // communication on this queue. The best we can do is not to corrupt guest memory.
        if (available_ring_idx >= _size)
            return Errno::NOTRECOVERABLE;

        desc = Descriptor(_descriptor_base, available_ring_idx);
        return Errno::NONE;
    }

    // Returns the number of queue elements available for processing.
    uint16 DeviceQueue::get_available() const {
        return count_available(available_index());
    }

    // Returns the number of free queue elements.
    uint16 DeviceQueue::get_free() const {
        return count_free(available_index());
    }

    // This checks if used->index satisfies the used_event condition for host to generate interrupt.
    // Guest (Driver) can use used_event to suppress interrupts till a certain threshold.
    bool DeviceQueue::used_event_notify() const {
        uint16 used_evt = get_used_event();
        uint16 used_idx = _driven_idx;

        return static_cast<uint16>(used_idx - used_evt - 1) < static_cast<uint16>(used_idx - _prev);
    }

    // Host (Device) can check to see if interrupts are disabled by guest.
    bool DeviceQueue::interrupts_disabled(void) const {
        return (_available.flags() & VIRTQ_AVAIL_NO_INTERRUPT) != 0;
    }

    // Host (Device) can suppress notifications using these routines.
    void DeviceQueue::enable_notifications() {
        _used.set_flags(0);
    }
    void DeviceQueue::disable_notifications() {
        _used.set_flags(_used.flags() | VIRTQ_USED_NO_NOTIFY);
    }
};

/** [Virtio::DriverQueue] */
namespace Virtio {
    // \pre <[initialize_descriptor] has never been invoked with [desc_idx] before>
    //
    // NOTE: the ghost state will be used to enforce this.
    Virtio::Descriptor DriverQueue::initialize_descriptor(uint16 desc_idx) const {
        return Virtio::Descriptor(_descriptor_base, desc_idx);
    }

    // Marks a descriptor chain as available for host.
    // NOTE: This ignores the [len] argument - which is only needed for the [Virtio::DeviceQueue].
    //
    // cf. 2.6.13 Supplying Buffers to The Device
    // <https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-5300013>
    void DriverQueue::send(Descriptor &&desc, uint32) {
        // 2.6.13.1 (precondition) - Driver sets up the buffer in the descriptor table,
        //                           and [desc] is the root of that chain

        // 2.6.12.2 - Driver places the index of the head of the descriptor chain into the
        //            next ring entry of the available ring.
        //
        // NOTE: the implementation of this function inserts the appropriate synchronization.
        _prev = _driven_idx++;
        _available.set_ring(_prev % _size, desc.index());

        // 2.6.13.3 - Batching is allowed, but we don't do it

        // 2.6.13.4 - The driver performs a suitable memory barrier to ensure the device sees
        //            the updated descriptor table and available ring before the next step.
        Barrier::w_before_w();

        // 2.6.13.5 - The available index is increased by the number of descriptor chain heads
        //            added to the available ring.
        // 2.6.13.6 - The driver performs a suitable memory barrier to ensure that it updates
        // ^          the idx field before checking for notification suppression.
        //  \---- NOTE: the implementation of this function inserts the appropriate synchronization.
        _available.set_index(_driven_idx);

        // 2.6.13.7 - The driver send an available buffer notification if such notifications
        //            are not suppressed.
    }

    // "Receive" a used descriptor chain from the host.
    Errno DriverQueue::recv(Descriptor &desc) {
        // NOTE: the implementation of this function inserts the appropriate synchronization.
        uint16 used_idx = used_index();

        // NOTE: make sure to test whether or not there are any descriptors available
        // prior to modifying the shared memory in any way.
        if (count_available(used_idx) == 0)
            return Errno::NOENT;

        // To support interrupt/notification suppression features.
        // If VIRTIO_EVENT_IDX is negotiated, we want to receive a notification from device when it
        // makes new buffers available.
        set_used_event(used_idx);

        // The index retrieved from used ring is the head of descriptor chain used by host.
        // NOTE: the [static_cast<uint16>] is provably redundant due to the [... % _size],
        // but the compiler isn't smart enough to realize this.
        uint16 desc_idx = static_cast<uint16>(_used.ring(_idx++ % _size).id() % _size);
        desc = Descriptor(_descriptor_base, desc_idx);
        return Errno::NONE;
    }

    // Returns the number of queue elements available for processing.
    uint16 DriverQueue::get_available() const {
        return count_available(used_index());
    }

    // Returns the number of free queue elements.
    uint16 DriverQueue::get_free() const {
        return count_free(used_index());
    }

    bool DriverQueue::notifications_disabled(void) {
        return (_used.flags() & VIRTQ_USED_NO_NOTIFY) != 0;
    }

    // Host (Device) can suppress notifications using these routines.
    void DriverQueue::enable_interrupts() {
        _available.set_flags(0);
    }
    void DriverQueue::disable_interrupts() {
        _available.set_flags(_available.flags() | VIRTQ_AVAIL_NO_INTERRUPT);
    }

    static uint8 *allocz(size_t sz) {
        auto alloc_sz = align_up(sz, 4096);
        auto *p = new (nothrow) uint8[alloc_sz];
        if (p == nullptr)
            return nullptr;

        memset(p, 0, alloc_sz);
        return p;
    }

    template<typename T>
    static uint8 *create_region(uint16 num_entries) {
        return allocz(T::region_size_bytes(num_entries));
    }

    Errno DriverQueue::create_driver_queue(uint16 num_entries, Virtio::DriverQueue &out) {
        // Allocate descriptor region.
        auto *desc = create_region<Virtio::Descriptor>(num_entries);
        if (nullptr == desc)
            return Errno::NOMEM;

        // Allocate available region.
        auto *avail = create_region<Virtio::Available>(num_entries);
        if (nullptr == avail) {
            delete[] desc;
            return Errno::NOMEM;
        }

        // Allocate used region.
        auto *used = create_region<Virtio::Used>(num_entries);
        if (nullptr == used) {
            delete[] avail;
            delete[] desc;
            return Errno::NOMEM;
        }

        // Construct a DriverQueue using the allocated regions.
        out = Virtio::DriverQueue(desc, avail, used, num_entries);
        return Errno::NONE;
    }

    void DriverQueue::delete_driver_queue(Virtio::DriverQueue &queue) {
        auto *desc = reinterpret_cast<uint8 *>(queue.descriptor_addr());
        delete[] desc;

        auto *avail = reinterpret_cast<uint8 *>(queue.available_addr());
        delete[] avail;

        auto *used = reinterpret_cast<uint8 *>(queue.used_addr());
        delete[] used;

        queue.~DriverQueue();
    }
};
