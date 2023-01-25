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
    if (_callback)
        _callback->driver_ok();
}

void
Model::Virtio_block::signal() {
    if (_backend_connected)
        assert_irq();
}

void
Model::Virtio_block::reset(const VcpuCtx *ctx) {
    if (_virtio_block_callback)
        _virtio_block_callback->device_reset(ctx);

    reset_virtio();
}

void
Model::Virtio_block::shutdown(const VcpuCtx *ctx) {
    if (_virtio_block_callback)
        _virtio_block_callback->shutdown(ctx);
}
