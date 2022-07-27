/*
 * Copyright (c) 2021 BedRock Systems, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/virtio_sg.hpp>
#include <platform/bits.hpp>
#include <platform/log.hpp>

void
Virtio::Sg::Node::heuristically_track_written_bytes(size_t off, size_t size_bytes) {
    size_t local_prefix_written_bytes = _prefix_written_bytes;

    if (off <= local_prefix_written_bytes) {
        local_prefix_written_bytes += off + size_bytes - local_prefix_written_bytes;

        if (local_prefix_written_bytes < _prefix_written_bytes
            || UINT32_MAX < local_prefix_written_bytes) {
            local_prefix_written_bytes = UINT32_MAX;
        }

        // This is a provably redundant cast, but the compiler is not smart enough to notice this.
        _prefix_written_bytes = static_cast<uint32>(local_prefix_written_bytes);
    }
}

uint32
Virtio::Sg::Buffer::written_bytes_lowerbound_heuristic() const {
    uint32 lb = 0;

    for (auto it = begin(); it != end(); ++it) {
        auto *node = &(*it);

        if (not(node->flags & VIRTQ_DESC_WRITE_ONLY))
            continue;

        lb += node->_prefix_written_bytes;

        if (node->_prefix_written_bytes != node->length)
            break;
    }

    return lb;
}

void
Virtio::Sg::Buffer::print(const char *msg) const {
    INFO("[Virtio::Sg::Buffer::print] => %s", msg);
    uint16 idx = 0;
    for (Virtio::Sg::Buffer::Iterator i = begin(); i != end(); i.operator++()) {
        auto &node = *i;
        INFO("| DESCRIPTOR@%d: {address: 0x%llx} {length: %d} {flags: 0x%x} {next: %d}", idx++,
             node.address, node.length, node.flags, node.next);
    }
}

void
Virtio::Sg::Buffer::conclude_chain_use(Virtio::Queue &vq, bool send_incomplete) {
    if (_complete_chain || send_incomplete) {
        // Implicitly drop the rest of the descriptors in the chain.
        //
        // NOTE: justified in the op-model because /physically/ sending the head
        // of the (partial) chain also /logically/ sends the body.
        vq.send(cxx::move(_nodes[0]._desc), written_bytes_lowerbound_heuristic());
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
Virtio::Sg::Buffer::walk_chain_callback(Virtio::Queue &vq, void *extra,
                                        ChainWalkingCallback *callback) {
    Virtio::Descriptor root_desc;
    Errno err = vq.recv(root_desc);
    if (ENONE != err) {
        return err;
    }

    return walk_chain_callback(vq, cxx::move(root_desc), extra, callback);
}

// \pre "[this.reset(_)] has been invoked"
// \pre "[desc] derived from a [vq->recv] call which returned [ENONE] (i.e. it is the
//       root of a descriptor chain in [vq])"
Errno
Virtio::Sg::Buffer::walk_chain_callback(Virtio::Queue &vq, Virtio::Descriptor &&root_desc,
                                        void *extra, ChainWalkingCallback *callback) {
    // Use a more meaningful name internally
    Virtio::Descriptor &desc = root_desc;
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
        err = ENOMEM;
        callback->chain_walking_cb(err, 0, 0, 0, 0, extra);
        // Maybe still necessary, but this won't fix what we've been seeing.
        vq.send(cxx::move(root_desc), 0);
        return err;
    }

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
            err = ENOTRECOVERABLE;

            // NOTE: The constructor of [Virtio::Queue] ensures that the queue-size is
            // nonzero and the early-return guarded by the [_max_chain_length < vq.size()]
            // test ensures that - at this point - [0 < max_chain_length]. This means that
            // [_nodes[_max_chain_length - 1]] corresponds to the last descriptor
            // which was walked prior to discovering the loop in the chain.
            Virtio::Sg::Node &node = _nodes[_max_chain_length - 1];
            callback->chain_walking_cb(err, node.address, node.length, node.flags, node.next,
                                       extra);
            conclude_chain_use(vq, true);
            return err;
        }

        Virtio::Sg::Node &node = _nodes[_active_chain_length++];
        node._desc = cxx::move(desc);
        node._prefix_written_bytes = 0;

        // Read the [address]/[length] a single time each.
        node.address = node._desc.address();
        node.length = node._desc.length();
        _size_bytes += node.length;

        err = vq.next_in_chain(node._desc, node.flags, next_en, node.next, desc);

        if (node.flags & VIRTQ_DESC_WRITE_ONLY) {
            seen_writable = true;
        } else if (seen_writable) {
            err = ENOTRECOVERABLE;
        }

        callback->chain_walking_cb(err, node.address, node.length, node.flags, node.next, extra);

        if (err != ENONE) {
            conclude_chain_use(vq, true);
            return err;
        }
    } while (next_en);

    _complete_chain = true;
    return ENONE;
}

void
Virtio::Sg::Buffer::add_link(Virtio::Descriptor &&desc, uint64 address, uint32 length, uint16 flags,
                             uint16 next) {
    ASSERT(flags & VIRTQ_DESC_CONT_NEXT);
    add_descriptor(cxx::move(desc), address, length, flags, next);
}

void
Virtio::Sg::Buffer::add_final_link(Virtio::Descriptor &&desc, uint64 address, uint32 length,
                                   uint16 flags) {
    ASSERT(not(flags & VIRTQ_DESC_CONT_NEXT));
    add_descriptor(cxx::move(desc), address, length, flags, 0);
    _complete_chain = true;
}

// NOTE: This interface does not allow flag/next modifications.
void
Virtio::Sg::Buffer::modify_link(size_t chain_idx, uint64 address, uint32 length) {
    ASSERT(chain_idx < _active_chain_length);
    auto &node = _nodes[chain_idx];

    node.address = address;
    node.length = length;

    node._desc.set_address(address);
    node._desc.set_length(length);
}

Errno
Virtio::Sg::Buffer::ChainAccessor::copy_between_gpa(BulkCopier *copier, ChainAccessor *dst_accessor,
                                                    ChainAccessor *src_accessor,
                                                    const GPA &dst_addr, const GPA &src_addr,
                                                    size_t &size_bytes) {
    if (not copier || not dst_accessor || not src_accessor) {
        return EINVAL;
    }

    char *dst_va{nullptr};
    char *src_va{nullptr};
    Errno err;

    err = dst_accessor->gpa_to_va_write(dst_addr, size_bytes, dst_va);
    if (ENONE != err) {
        return err;
    }

    err = src_accessor->gpa_to_va(src_addr, size_bytes, src_va);
    if (ENONE != err) {
        return err;
    }

    copier->bulk_copy(dst_va, src_va, size_bytes);

    err = src_accessor->gpa_to_va_post(src_addr, size_bytes, src_va);
    if (ENONE != err) {
        return err;
    }

    err = dst_accessor->gpa_to_va_post_write(dst_addr, size_bytes, dst_va);
    if (ENONE != err) {
        return err;
    }

    return ENONE;
}

Errno
Virtio::Sg::Buffer::ChainAccessor::copy_from_gpa(BulkCopier *copier, char *dst_va,
                                                 const GPA &src_addr, size_t &size_bytes) {
    if (not copier) {
        return EINVAL;
    }

    char *src_va{nullptr};
    Errno err;

    err = gpa_to_va(src_addr, size_bytes, src_va);
    if (ENONE != err) {
        return err;
    }

    copier->bulk_copy(dst_va, src_va, size_bytes);

    err = gpa_to_va_post(src_addr, size_bytes, src_va);
    if (ENONE != err) {
        return err;
    }

    return ENONE;
}

Errno
Virtio::Sg::Buffer::ChainAccessor::copy_to_gpa(BulkCopier *copier, const GPA &dst_addr,
                                               const char *src_va, size_t &size_bytes) {
    if (not copier) {
        return EINVAL;
    }

    char *dst_va{nullptr};
    Errno err;

    err = gpa_to_va_write(dst_addr, size_bytes, dst_va);
    if (ENONE != err) {
        return err;
    }

    copier->bulk_copy(dst_va, src_va, size_bytes);

    err = gpa_to_va_post_write(dst_addr, size_bytes, dst_va);
    if (ENONE != err) {
        return err;
    }

    return ENONE;
}

template<typename T_LINEAR, bool LINEAR_TO_SG>
Errno
Virtio::Sg::Buffer::copy(ChainAccessor *accessor, Virtio::Sg::Buffer &sg, T_LINEAR *l,
                         size_t &size_bytes, size_t off, BulkCopier *copier) {
    if (not accessor) {
        return EINVAL;
    }

    if (sg.size_bytes() < size_bytes) {
        return ENOMEM;
    }

    auto default_copier = BulkCopierDefault();
    if (not copier) {
        copier = &default_copier;
    }

    size_t rem = size_bytes;
    size_bytes = 0;

    auto it = sg.find(off);
    if (it == sg.end()) {
        return ENOENT;
    }

    while (rem and it != sg.end()) {
        auto *node = &(*it);

        size_t n_copy = min(node->length - off, rem);

        // Register the read/write to the [sg] buffer (depending on whether it is
        // a copy /from/ or /to/ the buffer).
        if constexpr (LINEAR_TO_SG) {
            if (not(node->flags & VIRTQ_DESC_WRITE_ONLY)) {
                return EPERM;
            }

            // NOTE: this function ensures that [copier] is non-null which means
            // that any non-[ENONE] error code came from the address translation
            // itself. Clients who need access to the particular translation
            // which failed can instrument custom tracking within their overload(s)
            // of [Sg::Buffer::ChainAccessor].
            if (ENONE != accessor->copy_to_gpa(copier, node->address + off, l, n_copy)) {
                return EBADR;
            }

            node->heuristically_track_written_bytes(off, n_copy);
        } else {
            // TODO (BC-1016): Drivers can use this path to copy data out of write-only
            // descriptors. So cannot perform strict permission checks, unless we
            // differentiate between Device- and Driver-copies.

            // NOTE: this function ensures that [copier] is non-null which means
            // that any non-[ENONE] error code came from the address translation
            // itself. Clients who need access to the particular translation
            // which failed can instrument custom tracking within their overload(s)
            // of [Sg::Buffer::ChainAccessor].
            if (ENONE != accessor->copy_from_gpa(copier, l, node->address + off, n_copy)) {
                return EBADR;
            }
        }

        off = 0;
        rem -= n_copy;

        size_bytes += n_copy;
        l += n_copy;

        ++it;
    }

    return ENONE;
}

Errno
Virtio::Sg::Buffer::copy(ChainAccessor *dst_accessor, Virtio::Sg::Buffer &dst, const void *src,
                         size_t &size_bytes, size_t d_off, BulkCopier *copier) {
    return copy<const char, true>(dst_accessor, dst, static_cast<const char *>(src), size_bytes,
                                  d_off, copier);
}

Errno
Virtio::Sg::Buffer::copy(ChainAccessor *src_accessor, void *dst, Virtio::Sg::Buffer &src,
                         size_t &size_bytes, size_t s_off, BulkCopier *copier) {
    return copy<char, false>(src_accessor, src, static_cast<char *>(dst), size_bytes, s_off,
                             copier);
}

Errno // NOLINTNEXTLINE(readability-function-size)
Virtio::Sg::Buffer::copy(ChainAccessor *dst_accessor, ChainAccessor *src_accessor,
                         Virtio::Sg::Buffer &dst, Virtio::Sg::Buffer &src, size_t &size_bytes,
                         size_t d_off, size_t s_off, BulkCopier *copier) {
    if (not dst_accessor || not src_accessor) {
        return EINVAL;
    }

    if (dst.size_bytes() < size_bytes || src.size_bytes() < size_bytes) {
        return ENOMEM;
    }

    auto default_copier = BulkCopierDefault();
    if (not copier) {
        copier = &default_copier;
    }

    // We need to copy as much as asked.
    size_t rem = size_bytes;
    size_bytes = 0;

    auto d = dst.find(d_off);
    if (d == dst.end()) {
        return ENOENT;
    }

    auto s = src.find(s_off);
    if (s == src.end()) {
        return ENOENT;
    }

    // Iterate over both till we have copied all or any of the buffers is exhausted.
    while (rem and (d != dst.end()) and (s != src.end())) {
        size_t n_copy = min(rem, min(s->length - s_off, d->length - d_off));

        // TODO (BC-1016): Drivers can use this path to copy data out of write-only
        // descriptors. So we shouldn't perform strict permission checks, unless we
        // differentiate between Device- and Driver-copies.
        //
        // NOTE (JH): It seems that no existing driver code uses this particular code
        // path, but we still need to fix it in order for it to be verifiable.
        if (s->flags & VIRTQ_DESC_WRITE_ONLY || not(d->flags & VIRTQ_DESC_WRITE_ONLY)) {
            return EPERM;
        }

        // NOTE: this function ensures that [copier]/[dst_accessor]/[src_accessor]
        // are non-null which means that any non-[ENONE] error code came from the
        // address translation itself. Clients who need access to the particular
        // translation which failed can instrument custom tracking within their
        // overload(s) of [Sg::Buffer::ChainAccessor].
        Errno err = ChainAccessor::copy_between_gpa(copier, dst_accessor, src_accessor,
                                                    d->address + d_off, s->address + s_off, n_copy);
        if (ENONE != err) {
            return EBADR;
        }

        d->heuristically_track_written_bytes(d_off, n_copy);

        rem -= n_copy;
        size_bytes += n_copy;

        // Update the destination offset and check if we need to move to next node.
        d_off += n_copy;
        if (d_off == d->length) {
            ++d;
            d_off = 0;
        }

        // Update the source offset and check if we need to move to next node.
        s_off += n_copy;
        if (s_off == s->length) {
            ++s;
            s_off = 0;
        }
    }

    return ENONE;
}

Errno
Virtio::Sg::Buffer::descriptor_offset(size_t descriptor_chain_idx, size_t &offset) const {
    size_t off = 0;
    for (Virtio::Sg::Buffer::Iterator i = begin(); 0 < descriptor_chain_idx; ++i) {
        if (i == end()) {
            return EINVAL;
        }

        off += (*i).length;
        descriptor_chain_idx--;
    }

    offset = off;
    return ENONE;
}

Errno
Virtio::Sg::Buffer::init() {
    _nodes = new (nothrow) Virtio::Sg::Node[_max_chain_length];
    if (_nodes == nullptr)
        return ENOMEM;

    return ENONE;
}

Errno
Virtio::Sg::Buffer::root_desc_idx(uint16 &root_desc_idx) const {
    if (0 == _active_chain_length) {
        root_desc_idx = UINT16_MAX;
        return ENOENT;
    } else {
        root_desc_idx = _nodes[0]._desc.index();
        return ENONE;
    }
}

void
Virtio::Sg::Buffer::add_descriptor(Virtio::Descriptor &&desc, uint64 address, uint32 length,
                                   uint16 flags, uint16 next) {
    // Grab a reference to the next node
    Virtio::Sg::Node &node = _nodes[_active_chain_length++];

    // Do the shared-memory updates
    desc.set_address(address);
    desc.set_length(length);
    desc.set_flags(flags);
    desc.set_next(next);

    // Cache all of the values in the [node]
    node._desc = cxx::move(desc);
    node.address = address;
    node.length = length;
    node.flags = flags;
    node.next = next;
}

void
Virtio::Sg::Buffer::reset(void) {
    _active_chain_length = 0;
    _size_bytes = 0;
    _complete_chain = false;
}

Virtio::Sg::Buffer::Iterator
Virtio::Sg::Buffer::find(size_t &offset) const {
    if (offset > size_bytes())
        return end();

    if (offset == 0)
        return begin();

    size_t cur_off = 0;
    for (auto it = begin(); it != end(); ++it) {
        auto *node = &(*it);
        if (cur_off + node->length > offset) {
            // Return the local offset within this node corresponding to desired linear offset.
            offset = offset - cur_off;
            return it;
        }

        cur_off += node->length;
    }

    return end();
}

Virtio::Sg::Node *
Virtio::Sg::Buffer::operator[](size_t index) {
    if (index >= _active_chain_length)
        return nullptr;

    return &_nodes[index];
}

const Virtio::Sg::Node *
Virtio::Sg::Buffer::operator[](size_t index) const {
    if (index >= _active_chain_length)
        return nullptr;

    return &_nodes[index];
}
