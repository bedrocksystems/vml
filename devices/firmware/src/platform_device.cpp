/**
 * Copyright (c) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <log/log.hpp>
#include <model/platform_device.hpp>
#include <model/vcpu_types.hpp>
#include <pm_client.hpp>
#include <vbus/vbus.hpp>

Vbus::Err
Model::Platform_device::access(Vbus::Access access, const Vcpu_ctx *vcpu_ctx, mword off,
                               uint8 bytes, uint64 &res) {
    Pm::Access acc
        = (access == Vbus::READ) ? Pm::READ : ((access == Vbus::WRITE) ? Pm::WRITE : Pm::EXEC);

    INFO("Platform_device::access (%d) acc = %d (offset: 0x%lx  sz:val: (%x:0x%llx))", _reg_id, acc,
         off, bytes, res);

    Errno err = _plat_mgr->handle_mmio(vcpu_ctx->ctx, acc, off, bytes, res, _reg_id);
    if (err != Errno::ENONE) {
        INFO("Platform_device::access (%d) fail to access 0x%lx  -> err:%d", _reg_id, off, err);
    }
    return (err == Errno::ENONE) ? Vbus::OK : Vbus::ACCESS_ERR;
}
