/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/virtio_net.hpp>
#include <platform/types.hpp>

void
Model::VirtioNet::notify(uint32 const) {
    if (_backend_connected) {
        _sig->sig();
    }
}

void
Model::VirtioNet::driver_ok() {
    if (_callback != nullptr)
        _callback->driver_ok();
}

void
Model::VirtioNet::signal() {
    if (_backend_connected)
        assert_irq();
}

void
Model::VirtioNet::reset(const VcpuCtx *ctx) {
    if (_virtio_net_callback != nullptr)
        _virtio_net_callback->device_reset(ctx);

    reset_virtio();
}

void
Model::VirtioNet::shutdown() {
    if (_virtio_net_callback != nullptr)
        _virtio_net_callback->shutdown();
}

void
Model::VirtioNet::attach() {
    Model::IOMMUManagedDevice::attach();
    if (_virtio_net_callback != nullptr)
        _virtio_net_callback->attach();
}

void
Model::VirtioNet::detach() {
    Model::IOMMUManagedDevice::detach();
    if (_virtio_net_callback != nullptr)
        _virtio_net_callback->detach();
}

Errno
Model::VirtioNet::map(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::map(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_net_callback != nullptr)
        return _virtio_net_callback->map(m);

    return Errno::NONE;
}

Errno
Model::VirtioNet::unmap(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::unmap(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_net_callback != nullptr)
        return _virtio_net_callback->unmap(m);

    return Errno::NONE;
}
