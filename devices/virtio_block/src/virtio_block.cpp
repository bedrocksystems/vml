/**
 * Copyright (C) 2021-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <model/iommu_interface.hpp>
#include <model/virtio_block.hpp>
#include <model/virtio_common.hpp>
#include <platform/errno.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>

void
Model::VirtioBlock::notify(uint32 const) {
    if (_backend_connected) {
        _sig->sig();
    }
}

void
Model::VirtioBlock::driver_ok() {
    if (_callback != nullptr)
        _callback->driver_ok();
}

void
Model::VirtioBlock::signal() {
    if (_backend_connected)
        assert_irq();
}

void
Model::VirtioBlock::reset() {
    if (_virtio_block_callback != nullptr)
        _virtio_block_callback->device_reset();

    reset_virtio();
}

void
Model::VirtioBlock::shutdown() {
    if (_virtio_block_callback != nullptr)
        _virtio_block_callback->shutdown();
}

void
Model::VirtioBlock::attach() {
    Model::IOMMUManagedDevice::attach();
    if (_virtio_block_callback != nullptr)
        _virtio_block_callback->attach();
}

void
Model::VirtioBlock::detach() {
    Model::IOMMUManagedDevice::detach();
    if (_virtio_block_callback != nullptr)
        _virtio_block_callback->detach();
}

Errno
Model::VirtioBlock::map(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::map(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_block_callback != nullptr)
        return _virtio_block_callback->map(m);

    return Errno::NONE;
}

Errno
Model::VirtioBlock::unmap(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::unmap(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_block_callback != nullptr)
        return _virtio_block_callback->unmap(m);

    return Errno::NONE;
}
