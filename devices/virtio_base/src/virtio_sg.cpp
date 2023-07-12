/*
 * Copyright (c) 2021 BedRock Systems, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/virtio_sg.hpp>
#include <platform/bits.hpp>
#include <platform/log.hpp>

void
Virtio::Sg::DescMetadata::heuristically_track_written_bytes(size_t off, size_t size_bytes) {
    size_t local_prefix_written_bytes = _prefix_written_bytes;

    if (off <= local_prefix_written_bytes) {
        local_prefix_written_bytes = off + size_bytes;
        if (local_prefix_written_bytes < _prefix_written_bytes || UINT32_MAX < local_prefix_written_bytes) {
            local_prefix_written_bytes = UINT32_MAX;
        }

        // This is a provably redundant cast, but the compiler is not smart enough to notice this.
        _prefix_written_bytes = static_cast<uint32>(local_prefix_written_bytes);
    }
}

Errno
Virtio::Sg::Buffer::check_copy_configuration(size_t size_bytes, size_t &inout_offset,
                                             Virtio::Sg::Buffer::Iterator &out_it) const {
    if (this->size_bytes() < inout_offset + size_bytes) {
        ASSERT(false);
        return Errno::NOMEM;
    }

    out_it = this->find(inout_offset);
    if (out_it == this->end()) {
        ASSERT(false);
        return Errno::NOENT;
    }

    return Errno::NONE;
}

uint32
Virtio::Sg::Buffer::written_bytes_lowerbound_heuristic() const {
    uint32 lb = 0;

    for (auto it = begin(); it != end(); ++it) {
        auto *desc = it.desc_ptr();
        auto *meta = it.meta_ptr();

        if ((desc->flags & VIRTQ_DESC_WRITE_ONLY) == 0)
            continue;

        lb += meta->_prefix_written_bytes;

        if (meta->_prefix_written_bytes != desc->length)
            break;
    }

    return lb;
}

void
Virtio::Sg::Buffer::print(const char *msg) const {
    INFO("[Virtio::Sg::Buffer::print] => %s", msg);
    uint16 idx = 0;
    for (Virtio::Sg::Buffer::Iterator i = begin(); i != end(); i.operator++()) {
        auto &desc = i.desc_ref();
        auto &meta = i.meta_ref();

        INFO("| DESCRIPTOR@%d: {address: 0x%llx} {length: %d} {flags: 0x%x} {next: %d}", idx++, desc.address, desc.length,
             desc.flags, meta._original_next);
    }
}

void
Virtio::Sg::Buffer::conclude_chain_use(Virtio::Queue &vq, bool send_incomplete) {
    if (_complete_chain || send_incomplete) {
        // NOTE (JH): important to call this member before we move some of the
        // [Virtio::Descriptor] ownership into [vq.send].
        auto lb = written_bytes_lowerbound_heuristic();

        // Implicitly drop the rest of the descriptors in the chain.
        //
        // NOTE: justified in the op-model because /physically/ sending the head
        // of the (partial) chain also /logically/ sends the body.
        vq.send(cxx::move(_desc_chain_metadata[0]._desc), lb);
    }

    reset();
}

Errno
Virtio::Sg::Buffer::walk_chain(Virtio::Queue &vq) {
    ChainWalkingNop callback;
    return walk_chain_callback(vq, nullptr, &callback);
}

Errno
Virtio::Sg::Buffer::walk_chain(Virtio::Queue &vq, Virtio::Descriptor &&root_desc) {
    ChainWalkingNop callback;
    return walk_chain_callback(vq, cxx::move(root_desc), nullptr, &callback);
}

Errno
Virtio::Sg::Buffer::walk_chain_callback(Virtio::Queue &vq, void *extra, ChainWalkingCallback *callback) {
    Virtio::Descriptor root_desc;
    Errno err = vq.recv(root_desc);
    if (Errno::NONE != err) {
        return err;
    }

    return walk_chain_callback(vq, cxx::move(root_desc), extra, callback);
}

// \pre "[this.reset(_)] has been invoked"
// \pre "[desc] derived from a [vq->recv] call which returned [Errno::NONE] (i.e. it is the
//       root of a descriptor chain in [vq])"
Errno
Virtio::Sg::Buffer::walk_chain_callback(Virtio::Queue &vq, Virtio::Descriptor &&root_desc, void *extra,
                                        ChainWalkingCallback *callback) {
    // Use a more meaningful name internally
    Virtio::Descriptor &tmp_desc = root_desc;
    // This flag tracks whether a writable buffer has been encountered within this chain already;
    // "The driver MUST place any device-writable descriptor elements after any device-readable
    // descriptor elements." cf. 2.6.4.2
    // <https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-280004>
    bool seen_writable = false;
    // This flag tracks whether there is a [next] descriptor in the chain
    bool next_en = false;
    Errno err;

    if (_max_chain_length < vq.get_size()) {
        // Leave [desc] unchanged and signal that a larger
        // [Virtio::Sg::Buffer] is needed.
        //
        // NOTE: No need to [reset()] since that was a pre-condition
        // of invoking the function, and we haven't modified [this].
        err = Errno::NOMEM;
        callback->chain_walking_cb(err, 0, 0, 0, 0, extra);
        // Maybe still necessary, but this won't fix what we've been seeing.
        vq.send(cxx::move(root_desc), 0);
        return err;
    }

    _chain_for_device = vq.is_device_queue();

    do {
        // NOTE: we must've moved the head of the chain into the buffer already since virtio
        // queues can't be empty.
        if (_active_chain_length == _max_chain_length) {
            // Since we have already checked that [vq.get_size() <= _max_chain_length],
            // [_max_chain_length <= _active_chain_length] indicates that we've
            // discovered a looped descriptor chain (which is illegal within the virtio
            // queue protocol).
            //
            // NOTE: The effect of this observation can be limited to flushing
            // the problematic (partial) descriptor chain from the virtio queue
            // observational model (rather than leaving the op-model state
            // unconstrained).
            err = Errno::NOTRECOVERABLE;

            // NOTE: The constructor of [Virtio::Queue] ensures that the queue-size is
            // nonzero and the early-return guarded by the [_max_chain_length < vq.size()]
            // test ensures that - at this point - [0 < max_chain_length]. This means that
            // [_nodes[_max_chain_length - 1]] corresponds to the last descriptor
            // which was walked prior to discovering the loop in the chain.
            auto &desc = _desc_chain[_max_chain_length - 1];
            auto &meta = _desc_chain_metadata[_max_chain_length - 1];
            callback->chain_walking_cb(err, desc.address, desc.length, desc.flags, meta._original_next, extra);
            conclude_chain_use(vq, true);
            return err;
        }

        // Grab the nodes for this [entry] and linearize the link for [desc.next]
        auto &desc = _desc_chain[_active_chain_length];
        auto &meta = _desc_chain_metadata[_active_chain_length];
        desc.linear_next = ++_active_chain_length;

        meta._desc = cxx::move(tmp_desc);
        meta._prefix_written_bytes = 0;

        // Read the [address]/[length] a single time each.
        desc.address = meta._desc.address();
        desc.length = meta._desc.length();
        _size_bytes += desc.length;

        // Walk the chain - storing the "real" next index in the [meta._original_next] field
        err = vq.next_in_chain(meta._desc, desc.flags, next_en, meta._original_next, tmp_desc);

        if ((desc.flags & VIRTQ_DESC_WRITE_ONLY) != 0) {
            seen_writable = true;
        } else if (seen_writable) {
            err = Errno::NOTRECOVERABLE;
        }

        callback->chain_walking_cb(err, desc.address, desc.length, desc.flags, meta._original_next, extra);

        if (Errno::NONE != err) {
            conclude_chain_use(vq, true);
            return err;
        }
    } while (next_en);

    _complete_chain = true;
    return Errno::NONE;
}

void
Virtio::Sg::Buffer::add_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags, uint16 next) {
    ASSERT(flags & VIRTQ_DESC_CONT_NEXT);
    add_descriptor(cxx::move(desc), address, length, flags, next);
}

void
Virtio::Sg::Buffer::add_final_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags) {
    ASSERT(not(flags & VIRTQ_DESC_CONT_NEXT));
    add_descriptor(cxx::move(desc), address, length, flags, 0);
    _complete_chain = true;
}

// NOTE: This interface does not allow flag/next modifications.
void
Virtio::Sg::Buffer::modify_link(size_t chain_idx, uint64 address, uint32 length) {
    ASSERT(chain_idx < _active_chain_length);
    auto &desc = _desc_chain[chain_idx];
    auto &meta = _desc_chain_metadata[chain_idx];

    desc.address = address;
    desc.length = length;

    meta._desc.set_address(address);
    meta._desc.set_length(length);
}

Errno
Virtio::Sg::Buffer::ChainAccessor::copy_between_gpa(BulkCopier *copier, ChainAccessor &dst_accessor, ChainAccessor &src_accessor,
                                                    const GPA &dst_addr, const GPA &src_addr, size_t &size_bytes) {
    if (copier == nullptr) {
        return Errno::INVAL;
    }

    char *dst_va{nullptr};
    char *src_va{nullptr};
    Errno err;

    err = dst_accessor.gpa_to_va_write(dst_addr, size_bytes, dst_va);
    if (Errno::NONE != err) {
        dst_accessor.handle_translation_failure(false /* !is_src */, err, dst_addr.value(), size_bytes);
        return err;
    }

    err = src_accessor.gpa_to_va(src_addr, size_bytes, src_va);
    if (Errno::NONE != err) {
        src_accessor.handle_translation_failure(true /* is_src */, err, src_addr.value(), size_bytes);
        return err;
    }

    copier->bulk_copy(dst_va, src_va, size_bytes);

    err = src_accessor.gpa_to_va_post(src_addr, size_bytes, src_va);
    if (Errno::NONE != err) {
        src_accessor.handle_translation_post_failure(true /* is_src */, err, src_addr.value(), size_bytes);
        return err;
    }

    err = dst_accessor.gpa_to_va_post_write(dst_addr, size_bytes, dst_va);
    if (Errno::NONE != err) {
        dst_accessor.handle_translation_post_failure(false /* !is_src */, err, dst_addr.value(), size_bytes);
        return err;
    }

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::ChainAccessor::copy_from_gpa(BulkCopier *copier, char *dst_va, const GPA &src_addr, size_t &size_bytes) {
    if (copier == nullptr) {
        return Errno::INVAL;
    }

    char *src_va{nullptr};
    Errno err;

    err = gpa_to_va(src_addr, size_bytes, src_va);
    if (Errno::NONE != err) {
        this->handle_translation_failure(true /* is_src */, err, src_addr.value(), size_bytes);
        return err;
    }

    copier->bulk_copy(dst_va, src_va, size_bytes);

    err = gpa_to_va_post(src_addr, size_bytes, src_va);
    if (Errno::NONE != err) {
        this->handle_translation_post_failure(true /* is_src */, err, src_addr.value(), size_bytes);
        return err;
    }

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::ChainAccessor::copy_to_gpa(BulkCopier *copier, const GPA &dst_addr, const char *src_va, size_t &size_bytes) {
    if (copier == nullptr) {
        return Errno::INVAL;
    }

    char *dst_va{nullptr};
    Errno err;

    err = gpa_to_va_write(dst_addr, size_bytes, dst_va);
    if (Errno::NONE != err) {
        this->handle_translation_failure(false /* !is_src */, err, dst_addr.value(), size_bytes);
        return err;
    }

    copier->bulk_copy(dst_va, src_va, size_bytes);

    err = gpa_to_va_post_write(dst_addr, size_bytes, dst_va);
    if (Errno::NONE != err) {
        this->handle_translation_post_failure(false /* !is_src */, err, dst_addr.value(), size_bytes);
        return err;
    }

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::start_copy_to_sg_impl(Virtio::Sg::Buffer &dst) const {
    // NOTE: [try_end_copy_to_impl] does all of the work
    (void)dst;
    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::try_end_copy_to_sg_impl(Virtio::Sg::Buffer &dst, ChainAccessor &dst_accessor, ChainAccessor &src_accessor,
                                            size_t &bytes_copied, BulkCopier &copier) const {
    // We need to copy as much as asked.
    size_t rem = dst._async_copy_cookie->req_sz();
    size_t d_off = dst._async_copy_cookie->req_d_off();
    size_t s_off = dst._async_copy_cookie->req_s_off();
    bytes_copied = 0;

    Virtio::Sg::Buffer::Iterator d = dst.end();
    Errno err = dst.check_copy_configuration(rem, d_off, d);
    if (Errno::NONE != err) {
        return err;
    }

    Virtio::Sg::Buffer::Iterator s = this->end();
    err = this->check_copy_configuration(rem, s_off, s);
    if (Errno::NONE != err) {
        return err;
    }

    // Iterate over both till we have copied all or any of the buffers is exhausted.
    while ((rem != 0u) and (d != dst.end()) and (s != this->end())) {
        auto *d_desc = d.desc_ptr();
        auto *d_meta = d.meta_ptr();
        auto *s_desc = s.desc_ptr();

        size_t n_copy = min(rem, min(s_desc->length - s_off, d_desc->length - d_off));

        if (dst.should_only_read(d_desc->flags)) {
            return Errno::PERM;
        } else if (this->should_only_write(s_desc->flags)) {
            WARN("[Virtio::Sg::Buffer] Devices should only read from a writable descriptor for "
                 "debugging purposes.");
        }

        // NOTE: this function ensures that [copier]/[dst_accessor]/[src_accessor]
        // are non-null which means that any non-[Errno::NONE] error code came from the
        // address translation itself. Clients who need access to the particular
        // translation which failed can instrument custom tracking within their
        // overload(s) of [Sg::Buffer::ChainAccessor].
        err = ChainAccessor::copy_between_gpa(&copier, dst_accessor, src_accessor, d_desc->address + d_off,
                                              s_desc->address + s_off, n_copy);
        if (Errno::NONE != err) {
            return Errno::BADR;
        }

        d_meta->heuristically_track_written_bytes(d_off, n_copy);

        rem -= n_copy;
        bytes_copied += n_copy;

        // Update the destination offset and check if we need to move to next node.
        d_off += n_copy;
        if (d_off == d_desc->length) {
            ++d;
            d_off = 0;
        }

        // Update the source offset and check if we need to move to next node.
        s_off += n_copy;
        if (s_off == s_desc->length) {
            ++s;
            s_off = 0;
        }
    }

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::start_copy_to_sg(Virtio::Sg::Buffer &dst, size_t size_bytes, size_t d_off, size_t s_off) const {
    // NOTE: This error code isn't rich enough to determine the precise mismatch.
    if (_async_copy_cookie->is_dst() || dst._async_copy_cookie->in_use()) {
        return Errno::RBUSY;
    }

    // NOTE: the [&dst] and [this] pointers are only used for later equality testing,
    // so the [_async_copy_cookie] only needs to track [valid_ptr] for each.
    _async_copy_cookie->init_sg_src_to_sg_dst(&dst);
    dst._async_copy_cookie->init_sg_dst_from_sg_src(this, size_bytes, d_off, s_off);

    Errno err = start_copy_to_sg_impl(dst);

    if (Errno::NONE != err) {
        dst._async_copy_cookie->conclude_dst();
        _async_copy_cookie->conclude_src();
    }

    return err;
}

Errno
Virtio::Sg::Buffer::try_end_copy_to_sg(Virtio::Sg::Buffer &dst, ChainAccessor &dst_accessor, ChainAccessor &src_accessor,
                                       size_t &bytes_copied, BulkCopier *copier) const {
    // NOTE: This error code isn't rich enough to determine the precise mismatch.
    if (!_async_copy_cookie->is_src_to_matching_sg(&dst)) {
        return Errno::BADR;
    }

    Errno err;
    if (0 < dst._async_copy_cookie->req_sz()) {
        auto default_copier = BulkCopierDefault();
        if (copier == nullptr) {
            copier = &default_copier;
        }

        err = try_end_copy_to_sg_impl(dst, dst_accessor, src_accessor, bytes_copied, *copier);
    } else {
        err = Errno::NONE;
    }

    if (Errno::AGAIN != err) {
        dst._async_copy_cookie->conclude_dst();
        _async_copy_cookie->conclude_src();
    }

    return err;
}

Errno
Virtio::Sg::Buffer::copy_to_sg(Virtio::Sg::Buffer &dst, ChainAccessor &dst_accessor, ChainAccessor &src_accessor,
                               size_t &size_bytes, size_t d_off, size_t s_off, BulkCopier *copier) const {
    Errno err;

    err = start_copy_to_sg(dst, size_bytes, d_off, s_off);
    if (Errno::NONE != err) {
        return err;
    }

    /** NOTE: once we start working with "real" asynchronous implementations we'll likely need more than a fixed number of retries
     * **/
    size_t retries = 10;
    do {
        err = try_end_copy_to_sg(dst, dst_accessor, src_accessor, size_bytes, copier);
    } while (retries-- != 0ul && Errno::AGAIN == err);

    return err;
}

template<typename T_LINEAR, bool LINEAR_TO_SG, typename T_SG>
Errno
Virtio::Sg::Buffer::copy_fromto_linear_impl(T_SG *sg, ChainAccessor &accessor, T_LINEAR *l, size_t &bytes_copied,
                                            BulkCopier &copier) {
    // We need to copy as much as asked.
    size_t rem = sg->_async_copy_cookie->req_sz();
    size_t sg_off = LINEAR_TO_SG ? sg->_async_copy_cookie->req_d_off() : sg->_async_copy_cookie->req_s_off();
    bytes_copied = 0;

    Virtio::Sg::Buffer::Iterator it = sg->end();
    Errno err = sg->check_copy_configuration(rem, sg_off, it);
    if (Errno::NONE != err) {
        return err;
    }

    while (rem and it != sg->end()) {
        auto *desc = it.desc_ptr();
        auto *meta = it.meta_ptr();

        size_t n_copy = min(desc->length - sg_off, rem);

        // Register the read/write to the [sg] buffer (depending on whether it is
        // a copy /from/ or /to/ the buffer).
        if constexpr (LINEAR_TO_SG) {
            if (sg->should_only_read(desc->flags)) {
                return Errno::PERM;
            }

            // NOTE: this function ensures that [copier] is non-null which means
            // that any non-[Errno::NONE] error code came from the address translation
            // itself. Clients who need access to the particular translation
            // which failed can instrument custom tracking within their overload(s)
            // of [Sg::Buffer::ChainAccessor].
            if (Errno::NONE != accessor.copy_to_gpa(&copier, desc->address + sg_off, l, n_copy)) {
                return Errno::BADR;
            }

            meta->heuristically_track_written_bytes(sg_off, n_copy);
        } else {
            if (sg->should_only_write(desc->flags)) {
                WARN("[Virtio::Sg::Buffer] Devices should only read from a writable descriptor for "
                     "debugging purposes.");
            }

            // NOTE: this function ensures that [copier] is non-null which means
            // that any non-[Errno::NONE] error code came from the address translation
            // itself. Clients who need access to the particular translation
            // which failed can instrument custom tracking within their overload(s)
            // of [Sg::Buffer::ChainAccessor].
            if (Errno::NONE != accessor.copy_from_gpa(&copier, l, desc->address + sg_off, n_copy)) {
                return Errno::BADR;
            }
        }

        sg_off = 0;
        rem -= n_copy;

        bytes_copied += n_copy;
        l += n_copy;

        ++it;
    }

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::start_copy_to_linear_impl(void *dst) const {
    // NOTE: [try_end_copy_to_impl] does all of the work
    (void)dst;
    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::try_end_copy_to_linear_impl(void *dst, ChainAccessor &src_accessor, size_t &bytes_copied,
                                                BulkCopier &copier) const {
    return Virtio::Sg::Buffer::copy_fromto_linear_impl<char, false, const Virtio::Sg::Buffer>(
        this, src_accessor, static_cast<char *>(dst), bytes_copied, copier);
}

Errno
Virtio::Sg::Buffer::start_copy_to_linear(void *dst, size_t size_bytes, size_t s_off) const {
    if (_async_copy_cookie->is_dst()) {
        return Errno::RBUSY;
    }

    // NOTE: the [&dst] and [this] pointers are only used for later equality testing,
    // so the [_async_copy_cookie] only needs to track [valid_ptr] for each.
    _async_copy_cookie->init_sg_src_to_linear_dst(dst, size_bytes, s_off);

    Errno err = start_copy_to_linear_impl(dst);

    if (Errno::NONE != err) {
        _async_copy_cookie->conclude_src();
    }

    return err;
}

Errno
Virtio::Sg::Buffer::try_end_copy_to_linear(void *dst, ChainAccessor &src_accessor, size_t &bytes_copied,
                                           BulkCopier *copier) const {
    // NOTE: This error code isn't rich enough to determine the precise mismatch.
    if (!_async_copy_cookie->is_src_to_matching_linear(dst)) {
        return Errno::BADR;
    }

    Errno err;
    if (0 < _async_copy_cookie->req_sz()) {
        auto default_copier = BulkCopierDefault();
        if (copier == nullptr) {
            copier = &default_copier;
        }

        err = try_end_copy_to_linear_impl(dst, src_accessor, bytes_copied, *copier);
    } else {
        err = Errno::NONE;
    }

    if (Errno::AGAIN != err) {
        _async_copy_cookie->conclude_src();
    }

    return err;
}

Errno
Virtio::Sg::Buffer::copy_to_linear(void *dst, ChainAccessor &src_accessor, size_t &size_bytes, size_t s_off,
                                   BulkCopier *copier) const {
    Errno err;

    err = start_copy_to_linear(dst, size_bytes, s_off);
    if (Errno::NONE != err) {
        return err;
    }

    /** NOTE: once we start working with "real" asynchronous implementations we'll likely need more than a fixed number of retries
     * **/
    size_t retries = 10;
    do {
        err = try_end_copy_to_linear(dst, src_accessor, size_bytes, copier);
    } while (retries-- != 0ul && Errno::AGAIN == err);

    return err;
}

Errno
Virtio::Sg::Buffer::start_copy_from_linear_impl(const void *src) {
    // NOTE: [try_end_copy_to_impl] does all of the work
    (void)src;
    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::try_end_copy_from_linear_impl(const void *src, ChainAccessor &dst_accessor, size_t &bytes_copied,
                                                  BulkCopier &copier) {
    return Virtio::Sg::Buffer::copy_fromto_linear_impl<const char, true, Virtio::Sg::Buffer>(
        this, dst_accessor, static_cast<const char *>(src), bytes_copied, copier);
}

Errno
Virtio::Sg::Buffer::start_copy_from_linear(const void *src, size_t size_bytes, size_t d_off) {
    if (_async_copy_cookie->is_dst()) {
        return Errno::RBUSY;
    }

    // NOTE: the [&dst] and [this] pointers are only used for later equality testing,
    // so the [_async_copy_cookie] only needs to track [valid_ptr] for each.
    _async_copy_cookie->init_sg_dst_from_linear_src(src, size_bytes, d_off);

    Errno err = start_copy_from_linear_impl(src);

    if (Errno::NONE != err) {
        _async_copy_cookie->conclude_dst();
    }

    return err;
}

Errno
Virtio::Sg::Buffer::try_end_copy_from_linear(const void *src, ChainAccessor &dst_accessor, size_t &bytes_copied,
                                             BulkCopier *copier) {
    // NOTE: This error code isn't rich enough to determine the precise mismatch.
    if (!_async_copy_cookie->is_dst_from_matching_linear(src)) {
        return Errno::BADR;
    }

    Errno err;
    if (0 < _async_copy_cookie->req_sz()) {
        auto default_copier = BulkCopierDefault();
        if (copier == nullptr) {
            copier = &default_copier;
        }

        err = try_end_copy_from_linear_impl(src, dst_accessor, bytes_copied, *copier);
    } else {
        err = Errno::NONE;
    }

    if (Errno::AGAIN != err) {
        _async_copy_cookie->conclude_dst();
    }

    return err;
}

Errno
Virtio::Sg::Buffer::copy_from_linear(const void *src, ChainAccessor &dst_accessor, size_t &size_bytes, size_t d_off,
                                     BulkCopier *copier) {
    Errno err;

    err = start_copy_from_linear(src, size_bytes, d_off);
    if (Errno::NONE != err) {
        return err;
    }

    /** NOTE: once we start working with "real" asynchronous implementations we'll likely need more than a fixed number of retries
     * **/
    size_t retries = 10;
    do {
        err = try_end_copy_from_linear(src, dst_accessor, size_bytes, copier);
    } while (retries-- != 0ul && Errno::AGAIN == err);

    return err;
}

Errno
Virtio::Sg::Buffer::descriptor_offset(size_t descriptor_chain_idx, size_t &offset) const {
    size_t off = 0;
    for (Virtio::Sg::Buffer::Iterator i = begin(); 0 < descriptor_chain_idx; ++i) {
        if (i == end()) {
            return Errno::INVAL;
        }

        off += i.desc_ref().length;
        descriptor_chain_idx--;
    }

    offset = off;
    return Errno::NONE;
}

void
Virtio::Sg::Buffer::deinit_async_copy_cookie() {
    ASSERT(!_async_copy_cookie || !_async_copy_cookie->in_use());

    delete _async_copy_cookie;
    _async_copy_cookie = nullptr;
}
void
Virtio::Sg::Buffer::deinit_desc_chain() {
    delete[] _desc_chain;
    _desc_chain = nullptr;
}
void
Virtio::Sg::Buffer::deinit_desc_chain_metadata() {
    delete[] _desc_chain_metadata;
    _desc_chain_metadata = nullptr;
}

Virtio::Sg::Buffer::~Buffer() {
    deinit_async_copy_cookie();
    deinit_desc_chain();
    deinit_desc_chain_metadata();
}

Errno
Virtio::Sg::Buffer::init_async_copy_cookie() {
    _async_copy_cookie = new (nothrow) Virtio::Sg::Buffer::AsyncCopyCookie();
    if (_async_copy_cookie == nullptr)
        return Errno::NOMEM;

    return Errno::NONE;
}
Errno
Virtio::Sg::Buffer::init_desc_chain() {
    _desc_chain = new (nothrow) Virtio::Sg::LinearizedDesc[_max_chain_length];
    if (_desc_chain == nullptr)
        return Errno::NOMEM;

    return Errno::NONE;
}
Errno
Virtio::Sg::Buffer::init_desc_chain_metadata() {
    _desc_chain_metadata = new (nothrow) Virtio::Sg::DescMetadata[_max_chain_length];
    if (_desc_chain_metadata == nullptr)
        return Errno::NOMEM;

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::init() {
    Errno err = init_async_copy_cookie();
    if (err != Errno::NONE) {
        return err;
    }

    err = init_desc_chain();
    if (err != Errno::NONE) {
        deinit_async_copy_cookie();
        return err;
    }

    err = init_desc_chain_metadata();
    if (err != Errno::NONE) {
        deinit_async_copy_cookie();
        deinit_desc_chain();
        return err;
    }

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::root_desc_idx(uint16 &root_desc_idx) const {
    if (0 == _active_chain_length) {
        root_desc_idx = UINT16_MAX;
        return Errno::NOENT;
    } else {
        root_desc_idx = _desc_chain_metadata[0]._desc.index();
        return Errno::NONE;
    }
}

void
Virtio::Sg::Buffer::add_descriptor(Virtio::Descriptor &&new_desc, uint64 address, uint32 length, uint16 flags, uint16 next) {
    // Grab a reference to the next node
    auto &desc = _desc_chain[_active_chain_length];
    auto &meta = _desc_chain_metadata[_active_chain_length];

    // Set up the linearized next idx in the cache and store the real next
    desc.linear_next = ++_active_chain_length;
    meta._original_next = next;

    // Do the shared-memory updates
    new_desc.set_address(address);
    new_desc.set_length(length);
    new_desc.set_flags(flags);
    new_desc.set_next(next);

    // Cache all of the remaining values
    meta._desc = cxx::move(new_desc);
    desc.address = address;
    desc.length = length;
    desc.flags = flags;
}

void
Virtio::Sg::Buffer::reset(void) {
    _active_chain_length = 0;
    _size_bytes = 0;
    _complete_chain = false;
}

Virtio::Sg::Buffer::Iterator
Virtio::Sg::Buffer::find(size_t &inout_offset) const {
    if (inout_offset > size_bytes())
        return end();

    if (inout_offset == 0)
        return begin();

    size_t cur_off = 0;
    for (auto it = begin(); it != end(); ++it) {
        auto &desc = it.desc_ref();

        if (cur_off + desc.length > inout_offset) {
            // Return the local offset within this node corresponding to desired linear offset.
            inout_offset = inout_offset - cur_off;
            return it;
        }

        cur_off += desc.length;
    }

    return end();
}

Virtio::Sg::LinearizedDesc *
Virtio::Sg::Buffer::desc_ptr(size_t index) {
    if (index >= _active_chain_length)
        return nullptr;

    return &_desc_chain[index];
}

const Virtio::Sg::LinearizedDesc *
Virtio::Sg::Buffer::desc_ptr(size_t index) const {
    if (index >= _active_chain_length)
        return nullptr;

    return &_desc_chain[index];
}

Virtio::Sg::DescMetadata *
Virtio::Sg::Buffer::meta_ptr(size_t index) {
    if (index >= _active_chain_length)
        return nullptr;

    return &_desc_chain_metadata[index];
}

const Virtio::Sg::DescMetadata *
Virtio::Sg::Buffer::meta_ptr(size_t index) const {
    if (index >= _active_chain_length)
        return nullptr;

    return &_desc_chain_metadata[index];
}
