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
Model::Virtio_net::_notify(uint32 const) {
    if (_backend_connected) {
        _sem->release();
    }
}

void
Model::Virtio_net::_driver_ok() {
    if (_callback)
        _callback->driver_ok();
}

void
Model::Virtio_net::signal() {
    if (_backend_connected)
        _assert_irq();
}

void
Model::Virtio_net::reset(const Vcpu_ctx *ctx) {
    if (_virtio_net_callback)
        _virtio_net_callback->device_reset(ctx);

    _reset();
}

Vbus::Err
Model::Virtio_net::access(Vbus::Access const access, const Vcpu_ctx *vcpu_ctx, Vbus::Space,
                          mword const offset, uint8 const size, uint64 &value) {

    bool ok = false;

    if (access == Vbus::Access::WRITE)
        ok = mmio_write(vcpu_ctx->vcpu_id, offset, size, value);
    if (access == Vbus::Access::READ)
        ok = mmio_read(vcpu_ctx->vcpu_id, offset, size, value);

    return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
}

bool
Model::Virtio_net::mmio_write(Vcpu_id const, uint64 const offset, uint8 const access_size,
                              uint64 const value) {
    return _write(offset, access_size, value);
}

bool
Model::Virtio_net::mmio_read(Vcpu_id const, uint64 const offset, uint8 const access_size,
                             uint64 &value) const {
    return _read(offset, access_size, value);
}
