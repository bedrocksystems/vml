/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/virtio_block.hpp>
#include <platform/types.hpp>

void
Model::Virtio_block::notify(uint32 const) {
    if (_backend_connected) {
        _sig->sig();
    }
}

void
Model::Virtio_block::driver_ok() {
    if (_callback != nullptr)
        _callback->driver_ok();
}

void
Model::Virtio_block::signal() {
    if (_backend_connected)
        assert_irq();
}

void
Model::Virtio_block::reset(const VcpuCtx *ctx) {
    if (_virtio_block_callback != nullptr)
        _virtio_block_callback->device_reset(ctx);

    reset_virtio();
}

void
Model::Virtio_block::shutdown() {
    if (_virtio_block_callback != nullptr)
        _virtio_block_callback->shutdown();
}

void
Model::Virtio_block::attach() {
    Model::IOMMUManagedDevice::attach();
    if (_virtio_block_callback != nullptr)
        _virtio_block_callback->attach();
}

void
Model::Virtio_block::detach() {
    Model::IOMMUManagedDevice::detach();
    if (_virtio_block_callback != nullptr)
        _virtio_block_callback->detach();
}

Errno
Model::Virtio_block::map(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::map(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_block_callback != nullptr)
        return _virtio_block_callback->map(m);

    return Errno::NONE;
}

Errno
Model::Virtio_block::unmap(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::unmap(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_block_callback != nullptr)
        return _virtio_block_callback->unmap(m);

    return Errno::NONE;
}
