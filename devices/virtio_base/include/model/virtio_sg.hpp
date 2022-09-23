/*
 * Copyright (c) 2021 BedRock Systems, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/simple_as.hpp>
#include <model/virtqueue.hpp>
#include <platform/errno.hpp>
#include <platform/new.hpp>
#include <platform/string.hpp>
#include <platform/utility.hpp>

namespace Virtio {
    namespace Sg {
        struct Node;
        class Buffer;
    };
};

struct Virtio::Sg::Node {
public:
    friend class Virtio::Sg::Buffer;

    Node &operator=(const Node &) = delete;
    Node(const Node &) = delete;

    Node &operator=(Node &&other) {
        if (this != &other) {
            cxx::swap(address, other.address);
            cxx::swap(length, other.length);
            cxx::swap(flags, other.flags);
            cxx::swap(next, other.next);
            cxx::swap(_desc, other._desc);
            cxx::swap(_prefix_written_bytes, other._prefix_written_bytes);
        }
        return *this;
    }
    Node(Node &&other) {
        cxx::swap(address, other.address);
        cxx::swap(length, other.length);
        cxx::swap(flags, other.flags);
        cxx::swap(next, other.next);
        _desc = cxx::move(other._desc);
        cxx::swap(_prefix_written_bytes, other._prefix_written_bytes);
    }

    Node() {}
    ~Node() {}

private:
    void heuristically_track_written_bytes(size_t off, size_t size_bytes);

public:
    // We ensure only a single read/write to each field by caching the values locally
    // and keeping the underlying [Virtio::Descriptor] private.
    uint64 address{0};
    uint32 length{0};
    uint16 flags{0};
    uint16 next{0};

private:
    Virtio::Descriptor _desc{Virtio::Descriptor()};

    // Used entries contain a [len] field which is used by the Device to inform the Driver
    // of the /lowerbound/ on the number of bytes that it wrote into the /prefix/ of the
    // /writable portion/ of the buffer. Since this is a /lowerbound/ on the number
    // of bytes written into the prefix, we can heuristically track this value. In particular
    // each /writable/ [Virtio::Sg::Node] will track a "prefix range" corresponding to the
    // number of bytes written to the beginning of the payload-shard for its [_desc];
    // [conclude_chain_use] will heuristically coalesce mergeable per-[Sg::Node] "prefix range"s,
    // giving a "maximal prefix range" for the whole chain.
    //
    // NOTE: While buffers can be made extremely large via chaining, the [len] field
    // can only hold a [uint32].
    uint32 _prefix_written_bytes{0};
};

class Virtio::Sg::Buffer {
public:
    Buffer &operator=(const Buffer &) = delete;
    Buffer(const Buffer &) = delete;

    Buffer &operator=(Buffer &&other) {
        if (this != &other) {
            cxx::swap(_max_chain_length, other._max_chain_length);
            cxx::swap(_active_chain_length, other._active_chain_length);
            cxx::swap(_size_bytes, other._size_bytes);
            cxx::swap(_complete_chain, other._complete_chain);
            cxx::swap(_chain_for_device, other._chain_for_device);
            cxx::swap(_nodes, other._nodes);
        }
        return *this;
    }
    Buffer(Buffer &&other) {
        cxx::swap(_max_chain_length, other._max_chain_length);
        cxx::swap(_active_chain_length, other._active_chain_length);
        cxx::swap(_size_bytes, other._size_bytes);
        cxx::swap(_complete_chain, other._complete_chain);
        cxx::swap(_chain_for_device, other._chain_for_device);
        cxx::swap(_nodes, other._nodes);
    }

    explicit Buffer(uint16 m) : _max_chain_length(m) {}

    // Maybe move this to a [deinit] function
    ~Buffer() {
        delete[] _nodes;
        _nodes = nullptr;
    }

    void print(const char *msg) const;

    Errno init();

    Errno root_desc_idx(uint16 &root_desc_idx) const;

    // [buff.conclude_chain_use(vq)] will return the chain of [Virtio::Descriptor]s
    // held by [buff] to the [vq] provided as an argument - [buff.reset()]ting in
    // in the process.
    //
    // NOTE: This is an idempotent function.
    void conclude_chain_use(Virtio::Queue &vq) { conclude_chain_use(vq, false); }

private:
    uint32 written_bytes_lowerbound_heuristic() const;
    void conclude_chain_use(Virtio::Queue &vq, bool send_incomplete);

public:
    class ChainWalkingCallback {
    public:
        virtual ~ChainWalkingCallback() {}
        virtual void chain_walking_cb(Errno err, uint64 address, uint32 length, uint16 flags,
                                      uint16 next, void *extra)
            = 0;
    };

    Errno walk_chain(Virtio::Queue &vq);
    Errno walk_chain(Virtio::Queue &vq, Virtio::Descriptor &&root_desc);
    Errno walk_chain_callback(Virtio::Queue &vq, void *extra, ChainWalkingCallback *callback);

    // \pre "[this.reset(_)] has been invoked"
    // \pre "[desc] derived from a [vq->recv] call which returned [ENONE] (i.e. it is the
    //       root of a descriptor chain in [vq])"
    Errno walk_chain_callback(Virtio::Queue &vq, Virtio::Descriptor &&root_desc, void *extra,
                              ChainWalkingCallback *callback);

    void add_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags,
                  uint16 next);
    void add_final_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags);
    // NOTE: This interface does not allow flag/next modifications.
    void modify_link(size_t chain_idx, uint64 address, uint32 length);

    inline size_t max_chain_length(void) const { return _max_chain_length; }
    inline size_t active_chain_length(void) const { return _active_chain_length; }
    inline size_t size_bytes(void) const { return _size_bytes; }

    class BulkCopier {
    public:
        virtual ~BulkCopier() {}
        virtual void bulk_copy(char *dst, const char *src, size_t size_bytes) = 0;
    };

    class ChainAccessor : public GuestPhysicalToVirtual {
    public:
        virtual ~ChainAccessor() {}
        static Errno copy_between_gpa(BulkCopier *copier, ChainAccessor *dst_accessor,
                                      ChainAccessor *src_accessor, const GPA &dst_addr,
                                      const GPA &src_addr, size_t &size_bytes);
        Errno copy_from_gpa(BulkCopier *copier, char *dst_va, const GPA &src_addr,
                            size_t &size_bytes);
        Errno copy_to_gpa(BulkCopier *copier, const GPA &dst_addr, const char *src_va,
                          size_t &size_bytes);
    };

public:
    // Copy [size_bytes] bytes from [src] Sg::Buffer to [dst] Sg::Buffer.
    static Errno copy(ChainAccessor *dst_accessor, ChainAccessor *src_accessor,
                      Virtio::Sg::Buffer &dst, Virtio::Sg::Buffer &src, size_t &size_bytes,
                      size_t d_off = 0, size_t s_off = 0, BulkCopier *copier = nullptr);

    // Copy [size_bytes] bytes from a linear buffer to an Sg::Buffer.
    static Errno copy(ChainAccessor *dst_accessor, Virtio::Sg::Buffer &dst, const void *src,
                      size_t &size_bytes, size_t d_off = 0, BulkCopier *copier = nullptr);

    // Copt [size_bytes] bytes from an Sg::Buffer to linear buffer.
    static Errno copy(ChainAccessor *src_accessor, void *dst, Virtio::Sg::Buffer &src,
                      size_t &size_bytes, size_t s_off = 0, BulkCopier *copier = nullptr);

private:
    template<typename T_LINEAR, bool LINEAR_TO_SG>
    static Errno copy(ChainAccessor *accessor, Virtio::Sg::Buffer &sg, T_LINEAR *l,
                      size_t &size_bytes, size_t off, BulkCopier *copier);

public:
    class Iterator {
    public:
        Iterator &operator++() {
            _cur++;
            return *this;
        }

        bool operator==(const Iterator &r) const { return _cur == r._cur; }
        bool operator!=(const Iterator &r) const { return !(*this == r); }

        Virtio::Sg::Node &operator*() const { return *_cur; }
        Virtio::Sg::Node *operator->() const { return _cur; }

    private:
        friend Virtio::Sg::Buffer;
        explicit Iterator(Virtio::Sg::Node *cur) : _cur(cur) {}

        Virtio::Sg::Node *_cur;
    };

public:
    Iterator begin() const { return Iterator{&_nodes[0]}; }
    Iterator end() const { return Iterator{&_nodes[_active_chain_length]}; }
    Errno descriptor_offset(size_t descriptor_chain_idx, size_t &offset) const;

private:
    class ChainWalkingNop : public ChainWalkingCallback {
    public:
        ChainWalkingNop() {}
        ~ChainWalkingNop() override {}
        void chain_walking_cb(Errno, uint64, uint32, uint16, uint16, void *) override {}
    };

    class BulkCopierDefault : public BulkCopier {
    public:
        BulkCopierDefault() {}
        ~BulkCopierDefault() override {}
        void bulk_copy(char *dst, const char *src, size_t size_bytes) override {
            memcpy(dst, src, size_bytes);
        }
    };

    // Hoist some static checks out of [Sg::Buffer::copy] to reduce cognitive complexity to
    // an acceptable level.
    Errno check_copy_configuration(ChainAccessor *accessor, size_t size_bytes, size_t &inout_offset,
                                   Iterator &out_it) const;

    // Check whether reading from the descriptor buffer /should/ be allowed based
    // on the supplied [flags].
    //
    // NOTE: sometimes a payload read /may/ be allowed (e.g. when debugging a
    // [Virtio::DeviceQueue]) even with the incorrect flags.
    // [Virtio::Sg::Buffer::copy] interprets the result of this call appropriately.
    bool should_only_read(uint16 flags) const {
        return _chain_for_device ? not(flags & VIRTQ_DESC_WRITE_ONLY) : false;
    }

    // Check whether writing to the descriptor buffer /should/ be allowed based
    // on the supplied [flags].
    //
    //
    // NOTE: sometimes a payload read /may/ be allowed (e.g. when debugging a
    // [Virtio::DeviceQueue]) even with the incorrect flags.
    // [Virtio::Sg::Buffer::copy] interprets the result of this call appropriately.
    bool should_only_write(uint16 flags) const {
        return _chain_for_device ? (flags & VIRTQ_DESC_WRITE_ONLY) : false;
    }

    // Common addition of descriptors to the chain
    void add_descriptor(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags,
                        uint16 next);

    // Common cleanup which should be completed after (partial) chains
    // are dealt with.
    void reset(void);

    // Returns an iterator pointing to the node containing the linear data offset /and/
    // modifies [inout_offset] to the appropriate node-specific linear data offset.
    Iterator find(size_t &inout_offset) const;

    Virtio::Sg::Node *operator[](size_t index);
    const Virtio::Sg::Node *operator[](size_t index) const;

private:
    // [Virtio::Queue]s have a maximum size of (2^15 - 1) [Virtio::Descriptior]s, and
    // the exclusion of loops means that no chain can have a length longer than this.
    uint16 _max_chain_length{0};
    uint16 _active_chain_length{0};
    size_t _size_bytes{0};

    // Track whether or not the contents of [_nodes] corresponds to a
    // complete or partial chain of descriptors; [reset] uses this
    // to determine how to properly clean up the contents of [_nodes].
    bool _complete_chain{false};
    // NOTE: Only meaningful when [_complete_chain] is [true]
    bool _chain_for_device{false};
    // morally [Virtio::Sg::Node _nodes[_max_chain_length]], but we can't allow for
    // exceptions in constructors (and thus defer allocation until the
    // [init] function).
    Virtio::Sg::Node *_nodes{nullptr};
};
