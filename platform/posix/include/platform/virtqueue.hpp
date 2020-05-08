/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/atomic.hpp>
#include <platform/types.hpp>

namespace Virtio {
    struct Descriptor;
    struct Available;
    struct Used;
    struct Used_entry;
    struct Queue;
    class DeviceQueue;
};

enum VIRTQ_DESC : uint16 {
    VIRTQ_DESC_CONT_NEXT = 0x1,
};

struct Virtio::Descriptor {
    static constexpr uint32 size(uint32 const max_elements) { return 16 * max_elements; }

    uint64 address; // Buffer guest physical address
    uint32 length;  // Buffer length;
    uint16 flags;   // Chained | write/read | indirect.
    uint16 next;    // Only valid if flags mark this descriptor as chained.
};

struct Virtio::Used_entry {
    uint32 id;
    uint32 length;
};

// Guest writes and host reads from available
struct Virtio::Available {
    static constexpr uint32 size(unsigned const max_elements) { return 4U + 2 * max_elements; }

    uint16 *ring() { return reinterpret_cast<uint16 *>(&_ring); }

    atomic<uint16> flags;
    atomic<uint16> index;
    uint16 _ring; // head of available ring array.
};

// Host writes and guest reads from used.
struct Virtio::Used {
    static constexpr uint32 size(uint32 const max_elements) { return 4U + 2 * 4 * max_elements; }

    Virtio::Used_entry *ring() { return reinterpret_cast<Virtio::Used_entry *>(&_ring); }

    atomic<uint16> flags;
    atomic<uint16> index;
    uint32 _ring; // head of used ring array.
};

struct Virtio::Queue {
    Virtio::Descriptor *descriptor{nullptr};
    Virtio::Available *available{nullptr};
    Virtio::Used *used{nullptr};
};

class Virtio::DeviceQueue {
public:
    DeviceQueue() = delete;

    DeviceQueue(Virtio::Queue *q, uint16 _size) : client(q), size(_size) {}

    // "Receive" a descriptor chain from the guest. It retrieves a chain of descriptors to be
    // processed and modified by the host. In virtio, a chain of descriptors is considered a single
    // buffer. available->index and used->index  are incremented once for each buffer. We expect
    // VIRTIO_IN_ORDER to be negotiated which will allow caller to increment the pointer to iterate
    // over the descriptor chain.
    Virtio::Descriptor *recv() {
        uint16 avail_idx = this->available_index();

        // To support interrupt/notification suppression features.
        // If VIRTIO_EVENT_IDX is negotiated, we want to receive a notification from guest when it
        // makes new buffers available.
        set_avail_event(avail_idx);
        if (count_available(avail_idx) == 0)
            return nullptr;

        // The index retrieved from available ring is the head of descriptor chain which needs to
        // be provided to used ring while marking a descriptor as used. Caller cannot manage it by
        // using it as a counter because the guest may use the same index again if it was reclaimed
        // before next transfer.
        return &client->descriptor[client->available->ring()[idx++ % size]];
    }

    // Send a descriptor chain back to guest.
    void send(Virtio::Descriptor *desc) {
        // We store the used index To support the used_event_notify feature which requires comparing
        // current and previous used index values.
        prev_used = used_index();

        client->used->ring()[prev_used % size].id = index_of(desc);
        client->used->ring()[prev_used % size].length = desc->length;

        // Increment the used index.
        update_used_index(1);
    }

private:
    inline uint16 count_available(uint16 avail_idx) const {
        // Index is a 16 bit free running counter. The max limit for ring size is 32768. Therefore,
        // a difference in local index copy and the available index set by guest is always
        // indicative of buffers available to be processed.
        return (avail_idx >= idx) ? uint16(avail_idx - idx) :
                                    uint16(avail_idx + size - 1 - (idx % size));
    }

    inline uint16 count_free(uint16 avail_idx) const {
        return static_cast<uint16>(size - count_available(avail_idx));
    }

    uint16 index_of(const Virtio::Descriptor *desc) const {
        return uint16((reinterpret_cast<mword>(desc) - reinterpret_cast<mword>(client->descriptor))
                      / sizeof(Virtio::Descriptor));
    }

    // Device manipulates avail_event field to suggest driver to suppress notifications till it has
    // added avail_event number of buffers to queue.
    inline void set_avail_event(uint16 index) {
        *reinterpret_cast<uint16 *>(&client->used->ring()[size]) = index;
    }

    inline uint16 used_index(void) const { return client->used->index.load(); }
    inline uint16 available_index(void) const { return client->available->index.load(); }
    inline void update_used_index(uint16 count) { client->used->index.fetch_add(count); }

private:
    Virtio::Queue *const client{nullptr};

    const uint16 size{0};

    // Local running index. Count from 0 - 65535 and wraps to zero.
    uint16 idx{0};

    // Stores previous used index value.
    uint16 prev_used{0};
};
