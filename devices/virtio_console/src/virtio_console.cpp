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
Model::VirtioMMIO_console::notify(uint32 const) {
    _sem->release();

    if (_queue[RX].queue().get_free())
        _sig_notify_empty_space.sig();
}

void
Model::VirtioMMIO_console::driver_ok() {
    _driver_initialized = true;

    if (_callback)
        _callback->driver_ok();
}

bool
Model::VirtioMMIO_console::to_guest(const char *buff, uint32 size) {
    if (!_queue[RX].constructed() || (!_driver_initialized))
        return false;

    uint32 buf_idx = 0;
    while (size) {
        auto *desc = _queue[RX].queue().recv();
        if (not desc) {
            return false;
        }

        char *dst = Model::SimpleAS::gpa_to_vmm_view(*_vbus, GPA(desc->address), desc->length);
        if (dst == nullptr) {
            _queue[RX].queue().send(desc);
            return false; /* outside guest physical memory */
        }

        uint32 n_copy = size <= desc->length ? size : desc->length;

        for (unsigned i = 0; i < n_copy; i++) {
            dst[i] = buff[buf_idx + i];
        }

        desc->length = n_copy;
        desc->flags &= static_cast<uint16>(~VIRTQ_DESC_CONT_NEXT);
        _queue[RX].queue().send(desc);

        size -= n_copy;
        buf_idx += n_copy;
    }
    assert_irq();

    return true;
}

const char *
Model::VirtioMMIO_console::from_guest(uint32 &size) {
    if (!_queue[TX].constructed() || (!_driver_initialized))
        return nullptr;

    _tx_desc = _queue[TX].queue().recv();
    if (not _tx_desc)
        return nullptr;

    char *vmm_addr
        = Model::SimpleAS::gpa_to_vmm_view(*_vbus, GPA(_tx_desc->address), _tx_desc->length);
    if (vmm_addr == nullptr) {
        _queue[TX].queue().send(_tx_desc);
        return nullptr; /* outside guest physical memory */
    }

    size = _tx_desc->length;
    return vmm_addr;
}

void
Model::VirtioMMIO_console::release_buffer() {
    if (!_queue[TX].constructed() || (!_driver_initialized))
        return;

    if (not _tx_desc)
        return;

    _queue[TX].queue().send(_tx_desc);
    assert_irq();
}

Vbus::Err
Model::VirtioMMIO_console::access(Vbus::Access const access, const VcpuCtx *vcpu_ctx, Vbus::Space,
                                  mword const offset, uint8 const size, uint64 &value) {

    bool ok = false;

    if (access == Vbus::Access::WRITE)
        ok = mmio_write(vcpu_ctx->vcpu_id, offset, size, value);
    if (access == Vbus::Access::READ)
        ok = mmio_read(vcpu_ctx->vcpu_id, offset, size, value);

    return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
}

bool
Model::VirtioMMIO_console::mmio_write(Vcpu_id const, uint64 const offset, uint8 const access_size,
                                      uint64 const value) {
    return write(offset, access_size, value);
}

bool
Model::VirtioMMIO_console::mmio_read(Vcpu_id const, uint64 const offset, uint8 const access_size,
                                     uint64 &value) const {
    return read(offset, access_size, value);
}
