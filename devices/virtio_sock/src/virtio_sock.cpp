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
    if (_callback)
        _callback->driver_ok();
}

void
Model::Virtio_sock::signal() {
    if (_backend_connected)
        assert_irq();
}

void
Model::Virtio_sock::reset(const VcpuCtx *ctx) {
    if (_virtio_sock_callback)
        _virtio_sock_callback->device_reset(ctx);

    reset_virtio();
}

Vbus::Err
Model::Virtio_sock::access(Vbus::Access const access, const VcpuCtx *vcpu_ctx, Vbus::Space,
                           mword const offset, uint8 const size, uint64 &value) {

    bool ok = false;

    if (access == Vbus::Access::WRITE)
        ok = mmio_write(vcpu_ctx->vcpu_id, offset, size, value);
    if (access == Vbus::Access::READ)
        ok = mmio_read(vcpu_ctx->vcpu_id, offset, size, value);

    return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
}

bool
Model::Virtio_sock::mmio_write(Vcpu_id const, uint64 const offset, uint8 const access_size,
                               uint64 const value) {
    return write(offset, access_size, value);
}

bool
Model::Virtio_sock::mmio_read(Vcpu_id const, uint64 const offset, uint8 const access_size,
                              uint64 &value) const {
    return read(offset, access_size, value);
}
