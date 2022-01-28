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

size_t
Model::Virtio_console::from_guest(char *out_buf, size_t sz) {
    if (!queue(TX).constructed() || (!_driver_initialized))
        return 0;

    size_t was_read = 0;

    while (sz != 0) {
        if (!_cur_tx.is_valid()) {
            /* get the next packet */
            Virtio::Descriptor *tx_desc = device_queue(TX).recv();
            if (tx_desc == nullptr)
                break;
            bool b = _cur_tx.construct(_vbus, tx_desc);
            if (!b) {
                device_queue(TX).send(tx_desc);
                break;
            }
        }

        size_t wr = _cur_tx.read(out_buf + was_read, sz);
        if (wr == 0) {
            device_queue(TX).send(_cur_tx.desc());
            _cur_tx.destruct();
            assert_irq();
        } else {
            sz -= wr;
            was_read += wr;
        }
    }

    return was_read;
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
