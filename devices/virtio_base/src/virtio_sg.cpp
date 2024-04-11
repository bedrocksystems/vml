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

void
Virtio::Sg::Buffer::heuristically_track_written_bytes(size_t off, size_t size_bytes) {
    // NOTE: [off] will contain the descriptor-local offset after [find] returns.
    Virtio::Sg::Buffer::Iterator it = find(off);
    // NOTE: caller ensures that [off]/[size_bytes] are compatible with [this]
    // chain and that the following loop terminates without error.
    while (size_bytes != 0ul) {
        auto *desc = it.desc_ptr();
        auto *meta = it.meta_ptr();

        uint32 desc_copy_sz = desc->length - static_cast<uint32>(off);
        if (desc_copy_sz <= size_bytes) {
            meta->heuristically_track_written_bytes(off, desc_copy_sz);
            size_bytes -= desc_copy_sz;
        } else {
            meta->heuristically_track_written_bytes(off, size_bytes);
            size_bytes = 0;
        }
        ++it;
        off = 0;
    }
}

uint32
Virtio::Sg::Buffer::written_bytes_lowerbound_heuristic() const {
    // NOTE: [walk_chain] ensures that chain lengths are no greater than [UINT32_MAX].
    uint32 lb = 0;

    if (is_writable()) {
        uint16 idx{UINT16_MAX};
        // NOTE: [is_writable() == true] ensures that [first_writable_desc]
        // return [Errno::NONE]
        (void)first_writable_desc(idx);
        while (idx < _active_chain_length) {
            auto &desc = _desc_chain[idx];
            auto &meta = _desc_chain_metadata[idx];
            idx++;

            // NOTE: [walk_chain] ensured that the entire suffix is writable (if
            // [is_writable() == true]).

            lb += meta._prefix_written_bytes;

            // stop once a break in the written bytes is encountered
            if (meta._prefix_written_bytes != desc.length)
                break;
        }
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

bool
Virtio::Sg::Buffer::should_send_head_descriptor(bool send_incomplete) const {
    return _complete_chain || send_incomplete;
}

void
Virtio::Sg::Buffer::conclude_chain_use(Virtio::Queue &vq, bool send_incomplete) {
    if (should_send_head_descriptor(send_incomplete)) {
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
        auto current_desc_idx = _active_chain_length;
        auto next_desc_idx = ++_active_chain_length;
        auto &desc = _desc_chain[current_desc_idx];
        auto &meta = _desc_chain_metadata[current_desc_idx];

        desc.linear_next = next_desc_idx;

        meta._desc = cxx::move(tmp_desc);
        meta._prefix_written_bytes = 0;

        // Read the [address]/[length] a single time each.
        desc.address = meta._desc.address();
        desc.length = meta._desc.length();
        _size_bytes += desc.length;

        // Walk the chain - storing the "real" next index in the [meta._original_next] field
        err = vq.next_in_chain(meta._desc, desc.flags, next_en, meta._original_next, tmp_desc);

        // Validate VIRTIO requirements & store info about readable/writable portions of the chain
        // "The driver MUST place any device-writable descriptor elements after any device-readable
        // descriptor elements." cf. 2.6.4.2
        // <https://docs.oasis-open.org/virtio/virtio/v1.1/cs01/virtio-v1.1-cs01.html#x1-280004>
        const bool desc_readable = (desc.flags & VIRTQ_DESC_WRITE_ONLY) == 0;
        if (Errno::NONE == err && desc_readable) {
            if (_seen_writable_desc) {
                err = Errno::NOTRECOVERABLE;
            } else {
                _seen_readable_desc = true;
            }
        } else if (Errno::NONE == err) { // [(desc.flags & VIRTQ_DESC_WRITE_ONLY) != 0] ==> [desc_writable]
            if (!_seen_writable_desc)
                _first_writable_desc = current_desc_idx;
            _seen_writable_desc = true;
        }

        // /-- NOTE: the VIRTIO standard doesn't explicitly forbid 0-length descriptors. However,
        // |   it does mention within "2.7.4 Message Framing" that:
        // |   > little sympathy will be given to drivers which create unreasonably-sized
        // |   > descriptors such as by dividing a network packet into 1500 single-byte
        // |   > descriptors!
        // |
        // |  Forbidding 0-length descriptors greatly simplifies the specification of this VIRTIO
        // |  code; our VIRTIO implementation "gives little sympathy" to drivers which include
        // |  0-length descriptors, something akin to creating 1-length descriptors.
        // |
        // v  (cf. <https://docs.oasis-open.org/virtio/virtio/v1.3/csd01/virtio-v1.3-csd01.pdf> 2.7.4)
        if (0 == desc.length || static_cast<size_t>(UINT32_MAX) < _size_bytes) {
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

Errno
Virtio::Sg::Buffer::add_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags, uint16 next) {
    const bool next_enabled = (flags & VIRTQ_DESC_CONT_NEXT) != 0;
    if (!next_enabled)
        return Errno::PERM;

    return add_descriptor(cxx::move(desc), address, length, flags, next);
}

Errno
Virtio::Sg::Buffer::add_final_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags) {
    const bool next_enabled = (flags & VIRTQ_DESC_CONT_NEXT) != 0;
    if (next_enabled)
        return Errno::PERM;

    Errno err = add_descriptor(cxx::move(desc), address, length, flags, 0);
    if (Errno::NONE == err) {
        _complete_chain = true;
    }
    return err;
}

// NOTE: This interface does not allow flag/next modifications.
Errno
Virtio::Sg::Buffer::modify_link(size_t chain_idx, uint64 address, uint32 length) {
    // Sanity check arguments
    if (_active_chain_length <= chain_idx) {
        return Errno::NOENT;
    }

    // /-- NOTE: the VIRTIO standard doesn't explicitly forbid 0-length descriptors. However,
    // |   it does mention within "2.7.4 Message Framing" that:
    // |   > little sympathy will be given to drivers which create unreasonably-sized
    // |   > descriptors such as by dividing a network packet into 1500 single-byte
    // |   > descriptors!
    // |
    // |  Forbidding 0-length descriptors greatly simplifies the specification of this VIRTIO
    // |  code; our VIRTIO implementation "gives little sympathy" to drivers which include
    // |  0-length descriptors, something akin to creating 1-length descriptors.
    // |
    // v  (cf. <https://docs.oasis-open.org/virtio/virtio/v1.3/csd01/virtio-v1.3-csd01.pdf> 2.7.4)
    if (0 == length) {
        return Errno::BADR;
    }

    auto &desc = _desc_chain[chain_idx];
    auto &meta = _desc_chain_metadata[chain_idx];

    // Validate VIRTIO requirements
    _size_bytes -= desc.length;
    if (static_cast<size_t>(UINT32_MAX) < _size_bytes || static_cast<size_t>(UINT32_MAX) < _size_bytes + length) {
        return Errno::BADR;
    }
    _size_bytes += length;

    // Update the descriptor
    desc.address = address;
    desc.length = length;

    meta._desc.set_address(address);
    meta._desc.set_length(length);

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::ChainAccessor::copy_between_vqa(BulkCopier *copier, ChainAccessor &dst_accessor, ChainAccessor &src_accessor,
                                                    uint64 dst_vqa, uint64 src_vqa, size_t &size_bytes) {
    if (copier == nullptr) {
        return Errno::INVAL;
    }

    char *dst_hva{nullptr};
    char *src_hva{nullptr};
    Errno err;

    err = dst_accessor.vq_addr_to_w_hva(dst_vqa, size_bytes, dst_hva);
    if (Errno::NONE != err) {
        dst_accessor.handle_translation_failure(false /* !is_src */, err, dst_vqa, size_bytes);
        return err;
    }

    err = src_accessor.vq_addr_to_r_hva(src_vqa, size_bytes, src_hva);
    if (Errno::NONE != err) {
        src_accessor.handle_translation_failure(true /* is_src */, err, src_vqa, size_bytes);
        return err;
    }

    copier->bulk_copy(dst_hva, src_hva, size_bytes);

    err = src_accessor.vq_addr_to_r_hva_post(src_vqa, size_bytes, src_hva);
    if (Errno::NONE != err) {
        src_accessor.handle_translation_post_failure(true /* is_src */, err, src_vqa, size_bytes);
        return err;
    }

    err = dst_accessor.vq_addr_to_w_hva_post(dst_vqa, size_bytes, dst_hva);
    if (Errno::NONE != err) {
        dst_accessor.handle_translation_post_failure(false /* !is_src */, err, dst_vqa, size_bytes);
        return err;
    }

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::ChainAccessor::copy_from_vqa(BulkCopier *copier, char *dst_hva, uint64 src_vqa, size_t &size_bytes) {
    if (copier == nullptr) {
        return Errno::INVAL;
    }

    char *src_hva{nullptr};
    Errno err;

    err = vq_addr_to_r_hva(src_vqa, size_bytes, src_hva);
    if (Errno::NONE != err) {
        this->handle_translation_failure(true /* is_src */, err, src_vqa, size_bytes);
        return err;
    }

    copier->bulk_copy(dst_hva, src_hva, size_bytes);

    err = vq_addr_to_r_hva_post(src_vqa, size_bytes, src_hva);
    if (Errno::NONE != err) {
        this->handle_translation_post_failure(true /* is_src */, err, src_vqa, size_bytes);
        return err;
    }

    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::ChainAccessor::copy_to_vqa(BulkCopier *copier, uint64 dst_vqa, const char *src_hva, size_t &size_bytes) {
    if (copier == nullptr) {
        return Errno::INVAL;
    }

    char *dst_hva{nullptr};
    Errno err;

    err = vq_addr_to_w_hva(dst_vqa, size_bytes, dst_hva);
    if (Errno::NONE != err) {
        this->handle_translation_failure(false /* !is_src */, err, dst_vqa, size_bytes);
        return err;
    }

    copier->bulk_copy(dst_hva, src_hva, size_bytes);

    err = vq_addr_to_w_hva_post(dst_vqa, size_bytes, dst_hva);
    if (Errno::NONE != err) {
        this->handle_translation_post_failure(false /* !is_src */, err, dst_vqa, size_bytes);
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
        err = ChainAccessor::copy_between_vqa(&copier, dst_accessor, src_accessor, d_desc->address + d_off,
                                              s_desc->address + s_off, n_copy);
        if (Errno::NONE != err) {
            return Errno::BADR;
        }

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
Virtio::Sg::Buffer::start_copy_to_sg(Virtio::Sg::Buffer &dst, size_t &size_bytes, size_t d_off, size_t s_off) const {
    // NOTE: This error code isn't rich enough to determine the precise mismatch.
    if (_async_copy_cookie->is_dst() || dst._async_copy_cookie->in_use()) {
        return Errno::RBUSY;
    }

    // NOTE: the [&dst] and [this] pointers are only used for later equality testing,
    // so the [_async_copy_cookie] only needs to track [valid_ptr] for each.
    _async_copy_cookie->init_sg_src_to_sg_dst(&dst);
    dst._async_copy_cookie->init_sg_dst_from_sg_src(this, size_bytes, d_off, s_off);
    size_bytes = 0;

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
    bytes_copied = 0;

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

    // Track the successfully written bytes
    //
    // NOTE: if clients don't write bytes in a strict prefix order then [Virtio::Sg::Buffer] will
    // under-report how many bytes were written.
    dst.heuristically_track_written_bytes(dst._async_copy_cookie->req_d_off(), bytes_copied);

    // If the copy concludes (successfully or w/non-[Errno::AGAIN] error) then we
    // conclude the use of the cookies, otherwise we record the partial copy progress
    if (Errno::AGAIN != err) {
        // Cleanup the async cookies
        dst._async_copy_cookie->conclude_dst();
        _async_copy_cookie->conclude_src();
    } else {
        // NOTE: for [sg->sg] copies, the destination cookie tracks all of the metadata
        dst._async_copy_cookie->record_bytes_copied(bytes_copied);
    }

    return err;
}

Errno
Virtio::Sg::Buffer::copy_to_sg(Virtio::Sg::Buffer &dst, ChainAccessor &dst_accessor, ChainAccessor &src_accessor,
                               size_t &size_bytes, size_t d_off, size_t s_off, BulkCopier *copier) const {
    Errno err;

    // v-- NOTE: [size_bytes] is passed by reference and reset to [0] in the [Errno::NONE] case
    err = start_copy_to_sg(dst, size_bytes, d_off, s_off);
    if (Errno::NONE != err) {
        return err;
    }

    /** NOTE: once we start working with "real" asynchronous implementations we'll likely need more than a fixed number of retries
     * **/
    size_t retries = 10;
    do {
        size_t bytes_copied{0};
        err = try_end_copy_to_sg(dst, dst_accessor, src_accessor, bytes_copied, copier);
        size_bytes += bytes_copied;
    } while (retries-- != 0ul && Errno::AGAIN == err);

    if (Errno::AGAIN == err) {
        dst._async_copy_cookie->conclude_dst();
        _async_copy_cookie->conclude_src();
    }

    return err;
}

// v-- NOTE: [T_LINEAR *l] already has the appropriate offset applied
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
            if (Errno::NONE != accessor.copy_to_vqa(&copier, desc->address + sg_off, l, n_copy)) {
                return Errno::BADR;
            }
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
            if (Errno::NONE != accessor.copy_from_vqa(&copier, l, desc->address + sg_off, n_copy)) {
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
Virtio::Sg::Buffer::try_end_copy_to_linear_impl(ChainAccessor &src_accessor, size_t &bytes_copied, BulkCopier &copier) const {
    return Virtio::Sg::Buffer::copy_fromto_linear_impl<char, false, const Virtio::Sg::Buffer>(
        this, src_accessor, _async_copy_cookie->req_linear_dst(), bytes_copied, copier);
}

Errno
Virtio::Sg::Buffer::start_copy_to_linear(void *dst, size_t &size_bytes, size_t s_off) const {
    if (_async_copy_cookie->is_dst()) {
        return Errno::RBUSY;
    }

    // NOTE: the [&dst] and [this] pointers are only used for later equality testing,
    // so the [_async_copy_cookie] only needs to track [valid_ptr] for each.
    _async_copy_cookie->init_sg_src_to_linear_dst(static_cast<char *>(dst), size_bytes, s_off);
    size_bytes = 0;

    Errno err = start_copy_to_linear_impl(dst);

    if (Errno::NONE != err) {
        _async_copy_cookie->conclude_src();
    }

    return err;
}

Errno
Virtio::Sg::Buffer::try_end_copy_to_linear(ChainAccessor &src_accessor, size_t &bytes_copied, BulkCopier *copier) const {
    bytes_copied = 0;

    // NOTE: so long as only a single linear src /or/ dst is supported for each buffer simultaneously, the [_async_copy_cookie]
    // information is enought to uniquely determine the relevant [char *] or [const char *] to use for a dereference.

    Errno err;
    if (0 < _async_copy_cookie->req_sz()) {
        auto default_copier = BulkCopierDefault();
        if (copier == nullptr) {
            copier = &default_copier;
        }

        err = try_end_copy_to_linear_impl(src_accessor, bytes_copied, *copier);
    } else {
        err = Errno::NONE;
    }

    // The destination is a linear buffer so we don't need to track information related to the number
    // of successfully written bytes

    // If the copy concludes (successfully or w/non-[Errno::AGAIN] error) then we
    // conclude the use of the cookies, otherwise we record the partial copy progress
    if (Errno::AGAIN != err) {
        _async_copy_cookie->conclude_src();
    } else {
        _async_copy_cookie->record_bytes_copied(bytes_copied);
    }

    return err;
}

Errno
Virtio::Sg::Buffer::copy_to_linear(void *dst, ChainAccessor &src_accessor, size_t &size_bytes, size_t s_off,
                                   BulkCopier *copier) const {
    Errno err;

    // v-- NOTE: [size_bytes] is passed by reference and reset to [0] in the [Errno::NONE] case
    err = start_copy_to_linear(dst, size_bytes, s_off);
    if (Errno::NONE != err) {
        return err;
    }

    /** NOTE: once we start working with "real" asynchronous implementations we'll likely need more than a fixed number of retries
     * **/
    size_t retries = 10;
    do {
        size_t bytes_copied{0};
        err = try_end_copy_to_linear(src_accessor, bytes_copied, copier);
        size_bytes += bytes_copied;
    } while (retries-- != 0ul && Errno::AGAIN == err);

    if (Errno::AGAIN == err) {
        _async_copy_cookie->conclude_src();
    }

    return err;
}

Errno
Virtio::Sg::Buffer::start_copy_from_linear_impl(const void *src) {
    // NOTE: [try_end_copy_to_impl] does all of the work
    (void)src;
    return Errno::NONE;
}

Errno
Virtio::Sg::Buffer::try_end_copy_from_linear_impl(ChainAccessor &dst_accessor, size_t &bytes_copied, BulkCopier &copier) {
    return Virtio::Sg::Buffer::copy_fromto_linear_impl<const char, true, Virtio::Sg::Buffer>(
        this, dst_accessor, _async_copy_cookie->req_linear_src(), bytes_copied, copier);
}

Errno
Virtio::Sg::Buffer::start_copy_from_linear(const void *src, size_t &size_bytes, size_t d_off) {
    if (_async_copy_cookie->is_dst()) {
        return Errno::RBUSY;
    }

    // NOTE: the [&dst] and [this] pointers are only used for later equality testing,
    // so the [_async_copy_cookie] only needs to track [valid_ptr] for each.
    _async_copy_cookie->init_sg_dst_from_linear_src(static_cast<const char *>(src), size_bytes, d_off);
    size_bytes = 0;

    Errno err = start_copy_from_linear_impl(src);

    if (Errno::NONE != err) {
        _async_copy_cookie->conclude_dst();
    }

    return err;
}

Errno
Virtio::Sg::Buffer::try_end_copy_from_linear(ChainAccessor &dst_accessor, size_t &bytes_copied, BulkCopier *copier) {
    bytes_copied = 0;

    // NOTE: so long as only a single linear src /or/ dst is supported for each buffer simultaneously, the [_async_copy_cookie]
    // information is enought to uniquely determine the relevant [char *] or [const char *] to use for a dereference.

    Errno err;
    if (0 < _async_copy_cookie->req_sz()) {
        auto default_copier = BulkCopierDefault();
        if (copier == nullptr) {
            copier = &default_copier;
        }

        err = try_end_copy_from_linear_impl(dst_accessor, bytes_copied, *copier);
    } else {
        err = Errno::NONE;
    }

    // Track the successfully written bytes
    //
    // NOTE: if clients don't write bytes in a strict prefix order then [Virtio::Sg::Buffer] will
    // under-report how many bytes were written.
    heuristically_track_written_bytes(_async_copy_cookie->req_d_off(), bytes_copied);

    if (Errno::AGAIN != err) {
        // Cleanup the async cookie
        _async_copy_cookie->conclude_dst();
    } else {
        // NOTE: for [sg->sg] copies, the destination cookie tracks all of the metadata
        _async_copy_cookie->record_bytes_copied(bytes_copied);
    }

    return err;
}

Errno
Virtio::Sg::Buffer::copy_from_linear(const void *src, ChainAccessor &dst_accessor, size_t &size_bytes, size_t d_off,
                                     BulkCopier *copier) {
    Errno err;

    // v-- NOTE: [size_bytes] is passed by reference and reset to [0] in the [Errno::NONE] case
    err = start_copy_from_linear(src, size_bytes, d_off);
    if (Errno::NONE != err) {
        return err;
    }

    /** NOTE: once we start working with "real" asynchronous implementations we'll likely need more than a fixed number of retries
     * **/
    size_t retries = 10;
    do {
        size_t bytes_copied{0};
        err = try_end_copy_from_linear(dst_accessor, bytes_copied, copier);
        size_bytes += bytes_copied;
    } while (retries-- != 0ul && Errno::AGAIN == err);

    if (Errno::AGAIN == err) {
        _async_copy_cookie->conclude_dst();
    }

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
    deinit();
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

void
Virtio::Sg::Buffer::deinit() {
    deinit_desc_chain_metadata();
    deinit_desc_chain();
    deinit_async_copy_cookie();
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

Errno
Virtio::Sg::Buffer::add_descriptor(Virtio::Descriptor &&new_desc, uint64 address, uint32 length, uint16 flags, uint16 next) {
    // Validate the arguments

    // /-- NOTE: the VIRTIO standard doesn't explicitly forbid 0-length descriptors. However,
    // |   it does mention within "2.7.4 Message Framing" that:
    // |   > little sympathy will be given to drivers which create unreasonably-sized
    // |   > descriptors such as by dividing a network packet into 1500 single-byte
    // |   > descriptors!
    // |
    // |  Forbidding 0-length descriptors greatly simplifies the specification of this VIRTIO
    // |  code; our VIRTIO implementation "gives little sympathy" to drivers which include
    // |  0-length descriptors, something akin to creating 1-length descriptors.
    // |
    // v  (cf. <https://docs.oasis-open.org/virtio/virtio/v1.3/csd01/virtio-v1.3-csd01.pdf> 2.7.4)
    if (0 == length) {
        return Errno::BADR;
    }

    // NOTE: we should be able to prove that [_size_bytes <= static_cast<size_t>(UINT32_MAX)]
    // as an invariant on the [Virtio::Sg::Buffer] class.
    if (static_cast<size_t>(UINT32_MAX) < _size_bytes || static_cast<size_t>(UINT32_MAX) < _size_bytes + length) {
        return Errno::BADR;
    }

    const bool readable = (flags & VIRTQ_DESC_WRITE_ONLY) == 0;
    if (_seen_writable_desc && readable) {
        return Errno::PERM;
    }

    // update [_size_bytes] and R/W information tracking
    _size_bytes += length;
    if (readable) {
        _seen_readable_desc = true;
    } else {
        if (!_seen_writable_desc)
            _first_writable_desc = _active_chain_length;
        _seen_writable_desc = true;
    }

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

    return Errno::NONE;
}

void
Virtio::Sg::Buffer::reset(void) {
    _active_chain_length = 0;
    _size_bytes = 0;
    _complete_chain = false;
    _seen_readable_desc = false;
    _seen_writable_desc = false;
    _first_writable_desc = UINT16_MAX;
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
