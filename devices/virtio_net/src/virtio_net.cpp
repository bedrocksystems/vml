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
Model::Virtio_net::notify(uint32 const) {
    if (_backend_connected) {
        _sig->sig();
    }
}

void
Model::Virtio_net::driver_ok() {
    if (_callback)
        _callback->driver_ok();
}

void
Model::Virtio_net::signal() {
    if (_backend_connected)
        assert_irq();
}

void
Model::Virtio_net::reset(const VcpuCtx *ctx) {
    if (_virtio_net_callback)
        _virtio_net_callback->device_reset(ctx);

    reset_virtio();
}
