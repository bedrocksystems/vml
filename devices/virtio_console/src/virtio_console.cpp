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
Model::Virtio_console::notify(uint32 const) {
    _sig_notify_event->sig();

    if (device_queue(RX).get_free())
        _sig_notify_empty_space.sig();
}

void
Model::Virtio_console::driver_ok() {
    _driver_initialized = true;

    if (_callback)
        _callback->driver_ok();
}

bool
Model::Virtio_console::to_guest(const char *buff, uint32 size) {
    if (!queue(RX).constructed() || (!_driver_initialized))
        return false;

    uint32 buf_idx = 0;
    while (size) {
        auto *desc = device_queue(RX).recv();
        if (not desc) {
            return false;
        }

        uint32 n_copy = size <= desc->length ? size : desc->length;
        Errno err = Model::SimpleAS::write_bus(*_vbus, GPA(desc->address), buff + buf_idx, n_copy);
        if (err != ENONE) {
            device_queue(RX).send(desc);
            return false; /* outside guest physical memory */
        }

        desc->length = n_copy;
        desc->flags &= static_cast<uint16>(~VIRTQ_DESC_CONT_NEXT);
        device_queue(RX).send(desc);

        size -= n_copy;
        buf_idx += n_copy;
    }
    assert_irq();

    return true;
}

const char *
Model::Virtio_console::from_guest(uint32 &size) {
    if (!queue(TX).constructed() || (!_driver_initialized))
        return nullptr;

    _tx_desc = device_queue(TX).recv();
    if (not _tx_desc)
        return nullptr;

    char *vmm_addr
        = Model::SimpleAS::gpa_to_vmm_view(*_vbus, GPA(_tx_desc->address), _tx_desc->length);
    if (vmm_addr == nullptr) {
        device_queue(TX).send(_tx_desc);
        return nullptr; /* outside guest physical memory */
    }

    size = _tx_desc->length;
    return vmm_addr;
}

void
Model::Virtio_console::release_buffer() {
    if (!queue(TX).constructed() || (!_driver_initialized))
        return;

    if (not _tx_desc)
        return;

    device_queue(TX).send(_tx_desc);
    assert_irq();
}
