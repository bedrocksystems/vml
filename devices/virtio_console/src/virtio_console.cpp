/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/virtio_console.hpp>
#include <platform/virtqueue.hpp>

void
Model::Virtio_console::_notify(uint32 const) {
    _sem->release();

    if (_queue[RX].queue().get_free())
        _sig_notify_empty_space.sig();
}

void
Model::Virtio_console::_driver_ok() {
    _driver_initialized = true;

    if (_callback)
        _callback->driver_ok();
}

bool
Model::Virtio_console::to_guest(char *buff, uint32 size) {
    if (!_queue[RX].constructed() || (!_driver_initialized))
        return false;

    uint32 buf_idx = 0;
    while (size) {
        auto desc = _queue[RX].queue().recv();
        if (not desc) {
            return false;
        }

        uint64 vmm_addr = 0;
        if (!_ram.local_address(desc->address, desc->length, vmm_addr)) {
            _queue[RX].queue().send(desc);
            return false; /* outside guest physical memory */
        }

        uint32 n_copy = size <= desc->length ? size : desc->length;

        char *dst = reinterpret_cast<char *>(vmm_addr);
        for (unsigned i = 0; i < n_copy; i++) {
            dst[i] = buff[buf_idx + i];
        }

        desc->length = n_copy;
        desc->flags &= static_cast<uint16>(~VIRTQ_DESC_CONT_NEXT);
        _queue[RX].queue().send(desc);

        size -= n_copy;
        buf_idx += n_copy;
    }
    _assert_irq();

    return true;
}

char *
Model::Virtio_console::from_guest(uint32 &size) {
    if (!_queue[TX].constructed() || (!_driver_initialized))
        return nullptr;

    _tx_desc = _queue[TX].queue().recv();
    if (not _tx_desc)
        return nullptr;

    uint64 vmm_addr = 0;
    if (!_ram.local_address(_tx_desc->address, _tx_desc->length, vmm_addr)) {
        _queue[TX].queue().send(_tx_desc);
        return nullptr; /* outside guest physical memory */
    }

    size = _tx_desc->length;
    return reinterpret_cast<char *>(vmm_addr);
}

void
Model::Virtio_console::release_buffer() {
    if (!_queue[TX].constructed() || (!_driver_initialized))
        return;

    if (not _tx_desc)
        return;

    _queue[TX].queue().send(_tx_desc);
    _assert_irq();
}

Vbus::Err
Model::Virtio_console::access(Vbus::Access const access, const Vcpu_ctx *vcpu_ctx, Vbus::Space,
                              mword const offset, uint8 const size, uint64 &value) {

    bool ok = false;

    if (access == Vbus::Access::WRITE)
        ok = mmio_write(vcpu_ctx->vcpu_id, offset, size, value);
    if (access == Vbus::Access::READ)
        ok = mmio_read(vcpu_ctx->vcpu_id, offset, size, value);

    return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
}

bool
Model::Virtio_console::mmio_write(Vcpu_id const, uint64 const offset, uint8 const access_size,
                                  uint64 const value) {
    return _write(offset, access_size, value);
}

bool
Model::Virtio_console::mmio_read(Vcpu_id const, uint64 const offset, uint8 const access_size,
                                 uint64 &value) const {
    return _read(offset, access_size, value);
}
