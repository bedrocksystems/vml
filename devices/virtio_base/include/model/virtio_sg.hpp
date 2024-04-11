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
    DescMetadata(DescMetadata &&other) : _desc(cxx::move(other._desc)) {

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
private:
    class AsyncCopyCookie {
        /** State */
    private:
        bool _copy_started{false};

        /** v-- NOTE: the following fields are only used when [_copy_started == true] */

        bool _other_is_sg{false};

        // NOTE: active destination Sg::Buffers may not be used as sources; active source
        // Sg::Buffers may be used as the source of other concurrent transactions - although
        // /either/ a single linear destination /or/ multiple Sg::Buffer destinations are
        // supported.
        bool _copy_is_src{false};
        // NOTE: only meaningful when [_copy_is_src == true]
        size_t _pending_dsts{0};

        //  /- NOTE: these fields are only used by:
        // |   1) [dst] Sg::Buffers, with linear /or/ Sg::Buffer [src]
        // v   2) [src] Sg::Buffers, with linear [dst]
        size_t _req_sz{~0ul};
        size_t _req_d_off{~0ul};
        size_t _req_s_off{~0ul};

        // /-- NOTES:
        // |   - when [_copy_started == true] at most one of these pointers will be non-null
        // |   - Since the [src] must track the cookie for a linear destination, only a single linear
        // v     destination can be supported currently.

        const Virtio::Sg::Buffer::AsyncCopyCookie *_cookie_src{nullptr};
        // v-- NOTE: [_linear_src]/[_linear_dst] pointers maintain ownership of the underlying memory
        const char *_linear_src{nullptr};
        char *_linear_dst{nullptr};

        /** Construction, Initialization and Destruction */
    public:
        AsyncCopyCookie() = default;
        ~AsyncCopyCookie() = default;

        // Copying not needed since we dynamically allocate cookies and then use pointers
        // to them.
        AsyncCopyCookie &operator=(const AsyncCopyCookie &) = delete;
        AsyncCopyCookie(const AsyncCopyCookie &) = delete;

        // Moving not needed since we dynamically allocate cookies and then use pointers
        // to them.
        AsyncCopyCookie &operator=(AsyncCopyCookie &&other) = delete;
        AsyncCopyCookie(AsyncCopyCookie &&other) = delete;

// BEGIN macros to make initialization code easier to read
#define IS_DST false
#define IS_SRC true
#define OTHER_SG true
#define OTHER_LINEAR false
        /** for [dst] in [src->dst] */
        void init_sg_dst_from_sg_src(const Sg::Buffer *src, size_t sz, size_t d_off, size_t s_off) {
            init_metadata(IS_DST, OTHER_SG, sz, d_off, s_off);
            _cookie_src = src->_async_copy_cookie;
        }
        /** for [src] in [src->dst] */
        void init_sg_src_to_sg_dst(const Sg::Buffer *dst) {
            // NOTE: the destination tracks the metadata for the transaction so the source doesn't
            // need to. This enables multiple destinations to be concurrently serviced by a single
            // source.
            init_status(IS_SRC, OTHER_SG);
            (void)dst;
        }
        /** for [src] in [src->linear] */
        void init_sg_src_to_linear_dst(char *dst, size_t sz, size_t s_off) {
            init_metadata(IS_SRC, OTHER_LINEAR, sz, 0ul, s_off);
            _linear_dst = dst;
        }
        /** for [dst] in [linear->dst] */
        void init_sg_dst_from_linear_src(const char *src, size_t sz, size_t d_off) {
            init_metadata(IS_DST, OTHER_LINEAR, sz, d_off, 0ul);
            _linear_src = src;
        }
#undef OTHER_LINEAR
#undef OTHER_SG
#undef SRC
#undef DST
        // END macros to make initialization code easier to read

        void record_bytes_copied(size_t bytes_copied) {
            ASSERT(bytes_copied <= _req_sz);
            ASSERT(_copy_started);
            _req_sz -= bytes_copied;
            _req_d_off += bytes_copied;
            _req_s_off += bytes_copied;
        }

        void conclude_dst(void) {
            ASSERT(_copy_started);
            ASSERT(!_copy_is_src);
            reset();
        }
        void conclude_src(void) {
            ASSERT(_copy_started);
            ASSERT(_copy_is_src);
            ASSERT(0 < _pending_dsts);

            // NOTE: only reset this source cookie if all of our destinations have been serviced
            if (--_pending_dsts == 0) {
                reset();
            }
        }

    private:
        void reset(void) {
            _copy_started = false;
            _other_is_sg = false;
            _copy_is_src = false;
            _pending_dsts = 0;
            _req_sz = ~0ul;
            _req_d_off = ~0ul;
            _req_s_off = ~0ul;
            _cookie_src = nullptr;
            _linear_src = nullptr;
            _linear_dst = nullptr;
        }

        void init_status(bool is_src, bool other_sg) {
            _copy_started = true;
            _other_is_sg = other_sg;
            _copy_is_src = is_src;
            // NOTE: [_pending_dsts] only used for sources; [_pending_dsts] acts as a reference
            // counter for sources - with a limit of 1 linear destination at a time.
            if (is_src) {
                if (other_sg) {
                    _pending_dsts++;
                } else {
                    _pending_dsts = 1;
                }
            } else {
                _pending_dsts = 0;
            }
        }
        void init_metadata(bool is_src, bool other_sg, size_t sz, size_t d_off, size_t s_off) {
            init_status(is_src, other_sg);
            _req_sz = sz;
            _req_d_off = d_off;
            _req_s_off = s_off;
        }

        /** General Utilities */
    public:
        inline size_t req_sz(void) const { return _req_sz; }
        inline size_t req_d_off(void) const { return _req_d_off; }
        inline size_t req_s_off(void) const { return _req_s_off; }
        // /-- NOTE: [req_linear_src]/[req_linear_dst] bake-in the appropriate offset addition
        inline const char *req_linear_src(void) { return _linear_src + req_s_off(); }
        inline char *req_linear_dst(void) { return _linear_dst + req_d_off(); }
        inline bool in_use(void) const { return _copy_started; }

        inline bool is_dst_from_sg(void) const { return in_use() && !_copy_is_src && _other_is_sg; }
        inline bool is_dst_from_linear(void) const { return in_use() && !_copy_is_src && !_other_is_sg; }
        inline bool is_dst(void) const { return is_dst_from_sg() || is_dst_from_linear(); }

        inline bool is_src_to_sg(void) const { return in_use() && _copy_is_src && _other_is_sg; }
        inline bool is_src_to_linear(void) const { return in_use() && _copy_is_src && !_other_is_sg; }
        inline bool is_src(void) const { return is_src_to_sg() || is_src_to_linear(); }

        // NOTE: when the src or dst is a linear buffer there will only be a single ongoing transaction
        // (and thus the cookie unambiguously determines the src or dst). However, for an [sg->sg] copy
        // there might be multiple destinations. Therefore, the cookies must be compared.
        bool is_dst_from_matching_cookie(const Virtio::Sg::Buffer::AsyncCopyCookie *src_cookie) const {
            bool candidate = is_dst_from_sg() && src_cookie->is_src_to_sg();
            return candidate && _cookie_src == src_cookie;
        }
        bool is_dst_from_matching_sg(const Virtio::Sg::Buffer *src) const {
            return is_dst_from_matching_cookie(src->_async_copy_cookie);
        }
        bool is_src_to_matching_sg(Virtio::Sg::Buffer *dst) const {
            return dst->_async_copy_cookie->is_dst_from_matching_cookie(this);
        }
    };

    /** State */
private:
    // [Virtio::Queue]s have a maximum size of (2^15 - 1) [Virtio::Descriptor]s, and
    // the exclusion of loops means that no chain can have a length longer than this.
    uint16 _max_chain_length{0};
    uint16 _active_chain_length{0};
    size_t _size_bytes{0};

    // Track whether or not the contents of [_desc_chain]/[_desc_chain_metadata]
    // correspond to a complete or partial chain of descriptors; [conclude_chain_use] uses
    // this to determine how to properly clean up.
    bool _complete_chain{false};
    // NOTE: Only meaningful when [_complete_chain] is [true]
    bool _chain_for_device{false};

    // Track which portions of the descriptor chain are readable and which portions are writable.
    //
    // NOTE: if [walk_chain] returns [Errno::NONE] then we know that the R/W permissions for the
    // chain are reasonable, namely, an (optional) readable prefix is followed by an (optional)
    // writable suffix. This well-formedness means that we only need to track:
    // 1) whether a readable and/or writable portion of the chain was encountered
    // 2) the index into the [_desc_chain]/[_desc_ahin_metadata] which separates the readable
    //    and writable portions of the chain.
    //
    // VIRTIO Standard:
    // "The driver MUST place any device-writable descriptor elements after any device-readable
    // descriptor elements." cf. 2.6.4.2
    // <https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-280004>
    bool _seen_readable_desc{false};
    bool _seen_writable_desc{false};
    // v-- NOTE: this field is only meaningful when [_seen_writable_desc == true]
    uint16 _first_writable_desc{UINT16_MAX};

    /** v-- NOTE: protected so that clients who override [init_XXX] can set these fields */
protected:
    /** After [init] returns [Errno::NONE], [_desc_chain] and [_desc_chain_metadata]
     *  both point to arrays with lengths equal to [_max_chain_length]
     */
    Virtio::Sg::LinearizedDesc *_desc_chain{nullptr};
    Virtio::Sg::DescMetadata *_desc_chain_metadata{nullptr};

    mutable Virtio::Sg::Buffer::AsyncCopyCookie *_async_copy_cookie{nullptr};

    /** Construction, Initialization and Destruction */
public:
    explicit Buffer(uint16 max_chain_length) : _max_chain_length(max_chain_length) {}

    Errno init();
    /** v-- NOTE: [deinit()] is idempotent */
    void deinit();
    virtual ~Buffer();

    // Copying is forbidden since [Sg::Buffer]s manage affine resources
    Buffer &operator=(const Buffer &) = delete;
    Buffer(const Buffer &) = delete;

    Buffer &operator=(Buffer &&other) {
        if (this != &other) {
            cxx::swap(_max_chain_length, other._max_chain_length);
            cxx::swap(_active_chain_length, other._active_chain_length);
            cxx::swap(_size_bytes, other._size_bytes);
            cxx::swap(_complete_chain, other._complete_chain);
            cxx::swap(_chain_for_device, other._chain_for_device);
            cxx::swap(_seen_readable_desc, other._seen_readable_desc);
            cxx::swap(_seen_writable_desc, other._seen_writable_desc);
            cxx::swap(_first_writable_desc, other._first_writable_desc);
            cxx::swap(_desc_chain, other._desc_chain);
            cxx::swap(_desc_chain_metadata, other._desc_chain_metadata);
            cxx::swap(_async_copy_cookie, other._async_copy_cookie);
        }
        return *this;
    }
    Buffer(Buffer &&other) {
        cxx::swap(_max_chain_length, other._max_chain_length);
        cxx::swap(_active_chain_length, other._active_chain_length);
        cxx::swap(_size_bytes, other._size_bytes);
        cxx::swap(_complete_chain, other._complete_chain);
        cxx::swap(_chain_for_device, other._chain_for_device);
        cxx::swap(_seen_readable_desc, other._seen_readable_desc);
        cxx::swap(_seen_writable_desc, other._seen_writable_desc);
        cxx::swap(_first_writable_desc, other._first_writable_desc);
        cxx::swap(_desc_chain, other._desc_chain);
        cxx::swap(_desc_chain_metadata, other._desc_chain_metadata);
        cxx::swap(_async_copy_cookie, other._async_copy_cookie);
    }

protected:
    /** Derived implementations might use alternative schemes to manage the memory
     *  backing the [_desc_chain] and [_desc_chain_metadata] arrays - which must
     *  have lengths matching [_max_chain_length].
     */
    virtual Errno init_async_copy_cookie();
    virtual Errno init_desc_chain();
    virtual Errno init_desc_chain_metadata();

    virtual void deinit_async_copy_cookie();
    virtual void deinit_desc_chain();
    virtual void deinit_desc_chain_metadata();

    /** General Utilities */
public:
    inline size_t max_chain_length(void) const { return _max_chain_length; }
    inline size_t active_chain_length(void) const { return _active_chain_length; }
    inline size_t size_bytes(void) const { return _size_bytes; }

    inline bool is_readable(void) const { return _seen_readable_desc; }
    inline bool is_writable(void) const { return _seen_writable_desc; }
    // v-- NOTE: [is_writable() && !is_readable() -> _first_writable_desc == 0]
    inline Errno first_writable_desc(uint16 &first_writable_desc) const {
        if (is_writable()) {
            first_writable_desc = _first_writable_desc;
            return Errno::NONE;
        } else {
            return Errno::PERM;
        }
    }
    inline Errno first_writable_byte(size_t &byte_offset) const {
        uint16 writable_linearized_desc_idx{UINT16_MAX};
        Errno err = first_writable_desc(writable_linearized_desc_idx);
        if (Errno::NONE == err) {
            byte_offset = 0;
            for (uint16 idx = 0; idx < writable_linearized_desc_idx; idx++) {
                byte_offset += _desc_chain[idx].length;
            }
        }
        return err;
    }

    // Print a message followed by the contents of any chain
    void print(const char *msg) const;
    Errno root_desc_idx(uint16 &root_desc_idx) const;
    Errno descriptor_offset(size_t descriptor_chain_idx, size_t &offset) const;

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
    Iterator begin() const { return Iterator{&_desc_chain[0], &_desc_chain_metadata[0]}; }
    Iterator end() const { return Iterator{&_desc_chain[_active_chain_length], &_desc_chain_metadata[_active_chain_length]}; }

    /** VIRTIO Driver Utilities */
    // This should only be used once the contents of the [_desc_chain] and [_desc_chain_metadata]
    // arrays are no longer needed.
    void reset(void);

    // NOTE: [Virtio::Sg::Buffer] enforces that constructed-chains don't violate driver obligations,
    // cf. comment above [Virtio::Sg::Buffer::add_descriptor]
    Errno add_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags, uint16 next);
    Errno add_final_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags);
    // NOTE: This interface does not allow flag/next modifications.
    Errno modify_link(size_t chain_idx, uint64 address, uint32 length);

protected:
    const Virtio::Sg::LinearizedDesc *desc_chain(void) const { return _desc_chain; }
    Errno get_copy_arguments_from_cookie(size_t &out_sz, size_t &out_d_off, size_t &out_s_off) const {
        if (!_async_copy_cookie->in_use()) {
            return Errno::NOENT;
        }

        if (_async_copy_cookie->is_src_to_sg()) {
            return Errno::BADR;
        }

        out_sz = _async_copy_cookie->req_sz();
        out_d_off = _async_copy_cookie->req_d_off();
        out_s_off = _async_copy_cookie->req_s_off();

        return Errno::NONE;
    }

private:
    // Common addition of descriptors to the chain
    //
    // NOTE: [Virtio::Sg::Buffer] enforces that constructed-chains don't violate driver obligations:
    // 1) chain-length <= Queue size -> no loops
    // 2) total byte size <= 2^32
    // 3) readable prefix; writable suffix
    Errno add_descriptor(Virtio::Descriptor &&new_desc, uint64 address, uint32 length, uint16 flags, uint16 next);

    // Returns an iterator pointing to the node containing the linear data offset /and/
    // modifies [inout_offset] to the appropriate node-specific linear data offset.
    Iterator find(size_t &inout_offset) const;

    Virtio::Sg::LinearizedDesc *desc_ptr(size_t index);
    const Virtio::Sg::LinearizedDesc *desc_ptr(size_t index) const;

    Virtio::Sg::DescMetadata *meta_ptr(size_t index);
    const Virtio::Sg::DescMetadata *meta_ptr(size_t index) const;

    /** Chain Return */
public:
    // [buff.conclude_chain_use(vq)] will return the chain of [Virtio::Descriptor]s
    // held by [buff] to the [vq] provided as an argument - [buff.reset()]ting in
    // in the process.
    //
    // NOTE: This is an idempotent function.
    void conclude_chain_use(Virtio::Queue &vq) { conclude_chain_use(vq, false); }

private:
    // NOTE: called after the copy succeeds - which means that the flag validation/size
    // checks have already been completed.
    void heuristically_track_written_bytes(size_t off, size_t size_bytes);
    uint32 written_bytes_lowerbound_heuristic() const;
    // v-- NOTE: serves as a hook for adding a specification
    bool inline should_send_head_descriptor(bool send_incomplete) const;
    void conclude_chain_use(Virtio::Queue &vq, bool send_incomplete);

    /** Chain Walking */
public:
    class ChainWalkingCallback {
    public:
        virtual ~ChainWalkingCallback() {}
        virtual void chain_walking_cb(Errno err, uint64 address, uint32 length, uint16 flags, uint16 next, void *extra) = 0;
    };

    // \pre "[this.reset(_)] has been invoked"
    // \pre "[desc] derived from a [vq->recv] call which returned [Errno::NONE] (i.e. it is the
    //       root of a descriptor chain in [vq])"
    virtual Errno walk_chain_callback(Virtio::Queue &vq, Virtio::Descriptor &&root_desc, void *extra,
                                      ChainWalkingCallback *callback);

    // NOTE: implemented directly in terms of [walk_chain_callback]
    Errno walk_chain(Virtio::Queue &vq);
    Errno walk_chain(Virtio::Queue &vq, Virtio::Descriptor &&root_desc);
    Errno walk_chain_callback(Virtio::Queue &vq, void *extra, ChainWalkingCallback *callback);

    /** (Asynchronous) Payload Manipulation */
    /** NOTE: the same dynamic type of [Sg::Buffer]s must be used with [copy_to(Sg::Buffer &, ...)] */
public:
    class BulkCopier {
    public:
        virtual ~BulkCopier() {}
        virtual void bulk_copy(char *dst, const char *src, size_t size_bytes) = 0;
    };

    class ChainAccessor : public Virtio::Queue::AddressTranslator {
    public:
        ~ChainAccessor() override {}
        static Errno copy_between_vqa(BulkCopier *copier, ChainAccessor &dst_accessor, ChainAccessor &src_accessor,
                                      uint64 dst_vqa, uint64 src_vqa, size_t &size_bytes);
        Errno copy_from_vqa(BulkCopier *copier, char *dst_hva, uint64 src_vqa, size_t &size_bytes);
        Errno copy_to_vqa(BulkCopier *copier, uint64 dst_vqa, const char *src_hva, size_t &size_bytes);

        // The follow methods are used in [copy_XXX_gpa] when the underlying
        // [Virtio::Queue::AddressTranslator] methods return [err != Errno::NONE].
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

    // BEGIN Asynchronous interface for copying from [this] Sg::Buffer to [dst] Sg::Buffer
private:
    virtual Errno start_copy_to_sg_impl(Virtio::Sg::Buffer &dst) const;
    virtual Errno try_end_copy_to_sg_impl(Virtio::Sg::Buffer &dst, ChainAccessor &dst_accessor, ChainAccessor &src_accessor,
                                          size_t &bytes_copied, BulkCopier &copier) const;

public:
    Errno start_copy_to_sg(Virtio::Sg::Buffer &dst, size_t &size_bytes, size_t d_off = 0, size_t s_off = 0) const;
    Errno try_end_copy_to_sg(Virtio::Sg::Buffer &dst, ChainAccessor &dst_accessor, ChainAccessor &src_accessor,
                             size_t &bytes_copied, BulkCopier *copier = nullptr) const;
    // END Asynchronous interface for copying from [this] Sg::Buffer to [dst] Sg::Buffer

    /** (Synchronously) Copy [size_bytes] from [this] Sg::Buffer to [dst] Sg::Buffer. */
    /** NOTE: [this] and [dst] are expected to have the same dynamic type */
    Errno copy_to_sg(Virtio::Sg::Buffer &dst, ChainAccessor &dst_accessor, ChainAccessor &src_accessor, size_t &size_bytes,
                     size_t d_off = 0, size_t s_off = 0, BulkCopier *copier = nullptr) const;

    // BEGIN Asynchronous interface for copying from [this] Sg::Buffer to [dst] linear buffer
private:
    virtual Errno start_copy_to_linear_impl(void *dst) const;
    virtual Errno try_end_copy_to_linear_impl(ChainAccessor &src_accessor, size_t &bytes_copied, BulkCopier &copier) const;

public:
    Errno start_copy_to_linear(void *dst, size_t &size_bytes, size_t s_off = 0) const;
    Errno try_end_copy_to_linear(ChainAccessor &src_accessor, size_t &bytes_copied, BulkCopier *copier = nullptr) const;
    // END Asynchronous interface for copying from [this] Sg::Buffer to [dst] linear buffer

    /** (Synchronously) Copy [size_bytes] from [src] Sg::Buffer to [dst] linear buffer. */
    Errno copy_to_linear(void *dst, ChainAccessor &src_accessor, size_t &size_bytes, size_t s_off = 0,
                         BulkCopier *copier = nullptr) const;

    // BEGIN Asynchronous interface for copying to [this] (dst) Sg::Buffer from [src] linear buffer
private:
    virtual Errno start_copy_from_linear_impl(const void *src);
    virtual Errno try_end_copy_from_linear_impl(ChainAccessor &dst_accessor, size_t &bytes_copied, BulkCopier &copier);

public:
    Errno start_copy_from_linear(const void *src, size_t &size_bytes, size_t d_off = 0);
    Errno try_end_copy_from_linear(ChainAccessor &dst_accessor, size_t &bytes_copied, BulkCopier *copier = nullptr);
    // END Asynchronous interface for copying to [this] (dst) Sg::Buffer from [src] linear buffer

    /** (Synchronously) Copy [size_bytes] bytes from [src] linear buffer to [this] (dst) Sg::Buffer. */
    Errno copy_from_linear(const void *src, ChainAccessor &dst_accessor, size_t &size_bytes, size_t d_off = 0,
                           BulkCopier *copier = nullptr);

private:
    // Hoist some static checks out of [Sg::Buffer::copy] to reduce cognitive complexity to
    // an acceptable level.
    Errno check_copy_configuration(size_t size_bytes, size_t &inout_offset, Iterator &out_it) const;

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

    template<typename T_LINEAR, bool LINEAR_TO_SG, typename T_SG>
    static Errno copy_fromto_linear_impl(T_SG *sg, ChainAccessor &accessor, T_LINEAR *l, size_t &bytes_copied,
                                         BulkCopier &copier);

    /** Default Instantiations  */
private:
    class ChainWalkingNop final : public ChainWalkingCallback {
    public:
        ChainWalkingNop() {}
        ~ChainWalkingNop() override {}
        void chain_walking_cb(Errno, uint64, uint32, uint16, uint16, void *) override {}
    };

    class BulkCopierDefault final : public BulkCopier {
    public:
        BulkCopierDefault() {}
        ~BulkCopierDefault() override {}
        void bulk_copy(char *dst, const char *src, size_t size_bytes) override { memcpy(dst, src, size_bytes); }
    };
};
