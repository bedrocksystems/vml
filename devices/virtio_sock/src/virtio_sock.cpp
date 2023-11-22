/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/virtio_sock.hpp>
#include <platform/types.hpp>

void
Model::Virtio_sock::notify(uint32 const) {
    if (_backend_connected) {
        _sig->sig();
    }
}

void
Model::Virtio_sock::driver_ok() {
    if (_callback != nullptr)
        _callback->driver_ok();
}

void
Model::Virtio_sock::signal() {
    if (_backend_connected)
        assert_irq();
}

void
Model::Virtio_sock::reset(const VcpuCtx *ctx) {
    if (_virtio_sock_callback != nullptr)
        _virtio_sock_callback->device_reset(ctx);

    reset_virtio();
}

void
Model::Virtio_sock::shutdown() {
    if (_virtio_sock_callback != nullptr)
        _virtio_sock_callback->shutdown();
}

void
Model::Virtio_sock::attach() {
    Model::IOMMUManagedDevice::attach();
    if (_virtio_sock_callback != nullptr)
        _virtio_sock_callback->attach();
}

void
Model::Virtio_sock::detach() {
    Model::IOMMUManagedDevice::detach();
    if (_virtio_sock_callback != nullptr)
        _virtio_sock_callback->detach();
}

Errno
Model::Virtio_sock::map(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::map(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_sock_callback != nullptr)
        return _virtio_sock_callback->map(m);

    return Errno::NONE;
}

Errno
Model::Virtio_sock::unmap(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::unmap(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_sock_callback != nullptr)
        return _virtio_sock_callback->unmap(m);

    return Errno::NONE;
}
