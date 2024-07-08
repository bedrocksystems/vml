/**
 * Copyright (C) 2021 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <model/virtio_sock.hpp>
#include <platform/types.hpp>

void
Model::VirtioSock::notify(uint32 const) {
    if (_backend_connected) {
        _sig->sig();
    }
}

void
Model::VirtioSock::driver_ok() {
    if (_callback != nullptr)
        _callback->driver_ok();
}

void
Model::VirtioSock::signal() {
    if (_backend_connected)
        assert_irq();
}

void
Model::VirtioSock::reset(const VcpuCtx *ctx) {
    if (_virtio_sock_callback != nullptr)
        _virtio_sock_callback->device_reset(ctx);

    reset_virtio();
}

void
Model::VirtioSock::shutdown() {
    if (_virtio_sock_callback != nullptr)
        _virtio_sock_callback->shutdown();
}

void
Model::VirtioSock::attach() {
    Model::IOMMUManagedDevice::attach();
    if (_virtio_sock_callback != nullptr)
        _virtio_sock_callback->attach();
}

void
Model::VirtioSock::detach() {
    Model::IOMMUManagedDevice::detach();
    if (_virtio_sock_callback != nullptr)
        _virtio_sock_callback->detach();
}

Errno
Model::VirtioSock::map(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::map(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_sock_callback != nullptr)
        return _virtio_sock_callback->map(m);

    return Errno::NONE;
}

Errno
Model::VirtioSock::unmap(const Model::IOMapping &m) {
    Errno err = Model::IOMMUManagedDevice::unmap(m);
    if (Errno::NONE != err)
        return err;
    if (_virtio_sock_callback != nullptr)
        return _virtio_sock_callback->unmap(m);

    return Errno::NONE;
}
