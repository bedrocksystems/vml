/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/virtio_console.hpp>
#include <model/virtqueue.hpp>

void
Model::Virtio_console::notify(uint32 const) {
    _sig_notify_event->sig();

    if (!queue(RX).constructed() || (!_driver_initialized))
        return;

    if (device_queue(RX).get_free() != 0u)
        _sig_notify_empty_space.sig();
}

void
Model::Virtio_console::driver_ok() {
    _driver_initialized = true;

    if (_callback != nullptr)
        _callback->driver_ok();
}

bool
Model::Virtio_console::to_guest(const char *buff, size_t size_bytes) {
    if (!queue(RX).constructed() || (!_driver_initialized))
        return false;

    size_t buf_idx = 0;
    while (size_bytes != 0u) {
        Errno err = _rx_buff.walk_chain(device_queue(RX));
        if (Errno::NONE != err) {
            assert_irq();
            return false;
        }

        size_t n_copy = min(size_bytes, _rx_buff.size_bytes());

        // NOTE: [Model::Virtio_console] is a concrete instantiation of
        // [Virtio::Sg::Buffer::ChainAccessor].
        err = _rx_buff.copy_from_linear(buff + buf_idx, *this, n_copy);
        _rx_buff.conclude_chain_use(device_queue(RX));
        assert_irq();

        if (Errno::NONE != err) {
            return false; /* outside guest physical memory */
        }

        size_bytes -= n_copy;
        buf_idx += n_copy;
    }

    return true;
}

size_t
Model::Virtio_console::from_guest(char *out_buf, size_t size_bytes) {
    if (!queue(TX).constructed() || (!_driver_initialized))
        return 0;

    size_t was_read = 0;
    Errno err = Errno::NONE;

    while (size_bytes != 0) {
        // NOTE: prior to any [walk_chain] - or right after a [conclude_chain_use] - [0 ==
        // _tx_buff.size_bytes()]
        if (0 == _tx_buff.size_bytes()) {
            _tx_buff_progress = 0;
            err = _tx_buff.walk_chain(device_queue(TX));
            if (Errno::NONE != err) {
                break;
            }
        }

        size_t n_copy = min(size_bytes, _tx_buff.size_bytes() - _tx_buff_progress);

        if (n_copy > 0) {
            // NOTE: [Model::Virtio_console] is a concrete instantiation of
            // [Virtio::Sg::Buffer::ChainAccessor].
            err = _tx_buff.copy_to_linear(out_buf + was_read, *this, n_copy, _tx_buff_progress);
        }
        if (Errno::NONE != err || 0 == n_copy) {
            _tx_buff.conclude_chain_use(device_queue(TX));
            assert_irq();

            if (Errno::NONE != err)
                return was_read;
        } else {
            _tx_buff_progress += n_copy;
            size_bytes -= n_copy;
            was_read += n_copy;
        }
    }

    return was_read;
}
