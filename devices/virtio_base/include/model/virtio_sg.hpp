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
        struct LinearizedDesc;
        struct DescMetadata;

        // [Sg::Buffer] contains [LinearizedDesc *_desc_chain] and [DescMetadata
        // *_desc_chain_metadata], whose elements are pairwise-related.
        class Buffer;
    };
};

// Template specializations for moves
// [Virtio::Sg::LinearizedDesc]
template typename cxx::remove_reference<Virtio::Sg::LinearizedDesc &>::type &&
cxx::move<Virtio::Sg::LinearizedDesc &>(Virtio::Sg::LinearizedDesc &t) noexcept;
template typename cxx::remove_reference<Virtio::Sg::LinearizedDesc>::type &&
cxx::move<Virtio::Sg::LinearizedDesc>(Virtio::Sg::LinearizedDesc &&t) noexcept;
// [Virtio::Sg::DescMetadata]
template typename cxx::remove_reference<Virtio::Sg::DescMetadata &>::type &&
cxx::move<Virtio::Sg::DescMetadata &>(Virtio::Sg::DescMetadata &t) noexcept;
template typename cxx::remove_reference<Virtio::Sg::DescMetadata>::type &&
cxx::move<Virtio::Sg::DescMetadata>(Virtio::Sg::DescMetadata &&t) noexcept;
// [Virtio::Sg::Buffer]
template typename cxx::remove_reference<Virtio::Sg::Buffer &>::type &&
cxx::move<Virtio::Sg::Buffer &>(Virtio::Sg::Buffer &t) noexcept;
template typename cxx::remove_reference<Virtio::Sg::Buffer>::type &&
cxx::move<Virtio::Sg::Buffer>(Virtio::Sg::Buffer &&t) noexcept;

struct Virtio::Sg::LinearizedDesc {
public:
    LinearizedDesc &operator=(const LinearizedDesc &) = delete;
    LinearizedDesc(const LinearizedDesc &) = delete;

    /* TODO BEFORE MERGE (JH): double check whether the [default]
       move operators are sensible.
     */
    LinearizedDesc &operator=(LinearizedDesc &&other) = default;
    LinearizedDesc(LinearizedDesc &&other) = default;

    LinearizedDesc() {}
    ~LinearizedDesc() {}

    // [Sg::Buffer] exposes a shadow-"Desriptor Table" which ensures that
    // the metadata constituting a descriptor chain is read only once. The
    // next links within this cached metadata are "linearized" - and related
    // to the real chain in shared memory via the corresponding
    // [DescMetadata] entry.
    uint64 address{0};
    uint32 length{0};
    uint16 flags{0};
    uint16 linear_next{0};
};

struct Virtio::Sg::DescMetadata {
public:
    friend class Virtio::Sg::Buffer;

    DescMetadata &operator=(const DescMetadata &) = delete;
    DescMetadata(const DescMetadata &) = delete;

    DescMetadata &operator=(DescMetadata &&other) {
        if (this != &other) {
            cxx::swap(_desc, other._desc);
            cxx::swap(_original_next, other._original_next);
            cxx::swap(_prefix_written_bytes, other._prefix_written_bytes);
        }
        return *this;
    }
    DescMetadata(DescMetadata &&other) {
        _desc = cxx::move(other._desc);
        cxx::swap(_original_next, other._original_next);
        cxx::swap(_prefix_written_bytes, other._prefix_written_bytes);
    }

    DescMetadata() {}
    ~DescMetadata() {}

private:
    void heuristically_track_written_bytes(size_t off, size_t size_bytes);

private:
    Virtio::Descriptor _desc{Virtio::Descriptor()};
    uint16 _original_next{0};

    // Used entries contain a [len] field which is used by the Device to inform the Driver
    // of the /lowerbound/ on the number of bytes that it wrote into the /prefix/ of the
    // /writable portion/ of the buffer. Since this is a /lowerbound/ on the number
    // of bytes written into the prefix, we can heuristically track this value. In particular
    // each /writable/ [Virtio::Sg::DescMetadata] will track a "prefix range" corresponding
    // to the number of bytes written to the beginning of the payload-shard for its [_desc];
    // [conclude_chain_use] will heuristically coalesce mergeable per-[Sg::DescMetadata]
    // "prefix range"s, giving a "maximal prefix range" for the whole chain.
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
            cxx::swap(_desc_chain, other._desc_chain);
            cxx::swap(_desc_chain_metadata, other._desc_chain_metadata);
        }
        return *this;
    }
    Buffer(Buffer &&other) {
        cxx::swap(_max_chain_length, other._max_chain_length);
        cxx::swap(_active_chain_length, other._active_chain_length);
        cxx::swap(_size_bytes, other._size_bytes);
        cxx::swap(_complete_chain, other._complete_chain);
        cxx::swap(_chain_for_device, other._chain_for_device);
        cxx::swap(_desc_chain, other._desc_chain);
        cxx::swap(_desc_chain_metadata, other._desc_chain_metadata);
    }

    explicit Buffer(uint16 m) : _max_chain_length(m) {}

    // Maybe move this to a [deinit] function
    ~Buffer() {
        delete[] _desc_chain;
        _desc_chain = nullptr;

        delete[] _desc_chain_metadata;
        _desc_chain_metadata = nullptr;
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
        virtual void chain_walking_cb(Errno err, uint64 address, uint32 length, uint16 flags, uint16 next, void *extra) = 0;
    };

    Errno walk_chain(Virtio::Queue &vq);
    Errno walk_chain(Virtio::Queue &vq, Virtio::Descriptor &&root_desc);
    Errno walk_chain_callback(Virtio::Queue &vq, void *extra, ChainWalkingCallback *callback);

    // \pre "[this.reset(_)] has been invoked"
    // \pre "[desc] derived from a [vq->recv] call which returned [Errno::NONE] (i.e. it is the
    //       root of a descriptor chain in [vq])"
    Errno walk_chain_callback(Virtio::Queue &vq, Virtio::Descriptor &&root_desc, void *extra, ChainWalkingCallback *callback);

    void add_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags, uint16 next);
    void add_final_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags);
    // NOTE: This interface does not allow flag/next modifications.
    void modify_link(size_t chain_idx, uint64 address, uint32 length);

    // Common cleanup which should be completed after (partial) chains
    // are dealt with.
    void reset(void);

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
        static Errno copy_between_gpa(BulkCopier *copier, ChainAccessor *dst_accessor, ChainAccessor *src_accessor,
                                      const GPA &dst_addr, const GPA &src_addr, size_t &size_bytes);
        Errno copy_from_gpa(BulkCopier *copier, char *dst_va, const GPA &src_addr, size_t &size_bytes);
        Errno copy_to_gpa(BulkCopier *copier, const GPA &dst_addr, const char *src_va, size_t &size_bytes);

        // The follow methods are used in [copy_XXX_gpa] when the underlying
        // [GuestPhysicalToVirtual] methods return [err != Errno::NONE].
    private:
        virtual void handle_translation_failure(bool is_src, Errno err, mword address, size_t sz) {
            (void)is_src;
            (void)err;
            (void)address;
            (void)sz;
        }
        virtual void handle_translation_post_failure(bool is_src, Errno err, mword address, size_t sz) {
            handle_translation_failure(is_src, err, address, sz);
        }
    };

public:
    // Copy [size_bytes] bytes from [src] Sg::Buffer to [dst] Sg::Buffer.
    static Errno copy(ChainAccessor *dst_accessor, ChainAccessor *src_accessor, Virtio::Sg::Buffer &dst,
                      const Virtio::Sg::Buffer &src, size_t &size_bytes, size_t d_off = 0, size_t s_off = 0,
                      BulkCopier *copier = nullptr);

    // Copy [size_bytes] bytes from a linear buffer to an Sg::Buffer.
    static Errno copy(ChainAccessor *dst_accessor, Virtio::Sg::Buffer &dst, const void *src, size_t &size_bytes, size_t d_off = 0,
                      BulkCopier *copier = nullptr);

    // Copt [size_bytes] bytes from an Sg::Buffer to linear buffer.
    static Errno copy(ChainAccessor *src_accessor, void *dst, const Virtio::Sg::Buffer &src, size_t &size_bytes, size_t s_off = 0,
                      BulkCopier *copier = nullptr);

private:
    template<typename T_LINEAR, bool LINEAR_TO_SG, typename SG_MAYBE_CONST>
    static Errno copy(ChainAccessor *accessor, SG_MAYBE_CONST &sg, T_LINEAR *l, size_t &size_bytes, size_t off,
                      BulkCopier *copier);

public:
    class Iterator {
    public:
        Iterator &operator++() {
            _cur_desc++;
            _cur_desc_metadata++;
            return *this;
        }

        bool operator==(const Iterator &r) const {
            return (_cur_desc == r._cur_desc) && (_cur_desc_metadata == r._cur_desc_metadata);
        }
        bool operator!=(const Iterator &r) const { return !(*this == r); }

        Virtio::Sg::LinearizedDesc &desc_ref() const { return *_cur_desc; }
        Virtio::Sg::LinearizedDesc *desc_ptr() const { return _cur_desc; }

        Virtio::Sg::DescMetadata &meta_ref() const { return *_cur_desc_metadata; }
        Virtio::Sg::DescMetadata *meta_ptr() const { return _cur_desc_metadata; }

    private:
        friend Virtio::Sg::Buffer;
        explicit Iterator(Virtio::Sg::LinearizedDesc *cur_desc, Virtio::Sg::DescMetadata *cur_desc_metadata)
            : _cur_desc(cur_desc), _cur_desc_metadata(cur_desc_metadata) {}

        Virtio::Sg::LinearizedDesc *_cur_desc;
        Virtio::Sg::DescMetadata *_cur_desc_metadata;
    };

public:
    Iterator begin() const { return Iterator{&_desc_chain[0], &_desc_chain_metadata[0]}; }
    Iterator end() const { return Iterator{&_desc_chain[_active_chain_length], &_desc_chain_metadata[_active_chain_length]}; }
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
        void bulk_copy(char *dst, const char *src, size_t size_bytes) override { memcpy(dst, src, size_bytes); }
    };

    // Hoist some static checks out of [Sg::Buffer::copy] to reduce cognitive complexity to
    // an acceptable level.
    Errno check_copy_configuration(ChainAccessor *accessor, size_t size_bytes, size_t &inout_offset, Iterator &out_it) const;

    // Check whether reading from the descriptor buffer /should/ be allowed based
    // on the supplied [flags].
    //
    // NOTE: sometimes a payload read /may/ be allowed (e.g. when debugging a
    // [Virtio::DeviceQueue]) even with the incorrect flags.
    // [Virtio::Sg::Buffer::copy] interprets the result of this call appropriately.
    bool should_only_read(uint16 flags) const { return _chain_for_device ? (flags & VIRTQ_DESC_WRITE_ONLY) == 0 : false; }

    // Check whether writing to the descriptor buffer /should/ be allowed based
    // on the supplied [flags].
    //
    //
    // NOTE: sometimes a payload read /may/ be allowed (e.g. when debugging a
    // [Virtio::DeviceQueue]) even with the incorrect flags.
    // [Virtio::Sg::Buffer::copy] interprets the result of this call appropriately.
    bool should_only_write(uint16 flags) const { return _chain_for_device ? (flags & VIRTQ_DESC_WRITE_ONLY) != 0 : false; }

    // Common addition of descriptors to the chain
    void add_descriptor(Virtio::Descriptor &&new_desc, uint64 address, uint32 length, uint16 flags, uint16 next);

    // Returns an iterator pointing to the node containing the linear data offset /and/
    // modifies [inout_offset] to the appropriate node-specific linear data offset.
    Iterator find(size_t &inout_offset) const;

    Virtio::Sg::LinearizedDesc *desc_ptr(size_t index);
    const Virtio::Sg::LinearizedDesc *desc_ptr(size_t index) const;

    Virtio::Sg::DescMetadata *meta_ptr(size_t index);
    const Virtio::Sg::DescMetadata *meta_ptr(size_t index) const;

private:
    // [Virtio::Queue]s have a maximum size of (2^15 - 1) [Virtio::Descriptor]s, and
    // the exclusion of loops means that no chain can have a length longer than this.
    uint16 _max_chain_length{0};
    uint16 _active_chain_length{0};
    size_t _size_bytes{0};

    // Track whether or not the contents of [_desc_chain]/[_desc_chain_metadata]
    // correspond to a complete or partial chain of descriptors; [reset] uses this
    // to determine how to properly clean up the contents of [_desc_chain]/[_desc_chain_metadata].
    bool _complete_chain{false};
    // NOTE: Only meaningful when [_complete_chain] is [true]
    bool _chain_for_device{false};
    // Both of these are morally [XXX xxx[_max_chain_length]], but we can't allow for
    // exceptions in constructors (and thus defer allocation until the [init] function).
    Virtio::Sg::LinearizedDesc *_desc_chain{nullptr};
    Virtio::Sg::DescMetadata *_desc_chain_metadata{nullptr};
};
