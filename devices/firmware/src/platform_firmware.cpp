/**
 * Copyright (c) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <model/platform_firmware.hpp>
#include <pm_client.hpp>

bool
Model::Firmware::handle_smc(const Vcpu_ctx *vcpu_ctx, mword p0, mword p1, mword p2, mword p3,
                            mword p4, mword p5, mword p6, mword (&ret)[4]) const {
    const Pm::Smc_args in(p0, p1, p2, p3, p4, p5, p6);
    Pm::Smc_ret out;

    Errno err = _plat_mgr->handle_smc(vcpu_ctx->ctx, in, out);
    if (err != Errno::ENONE) {
        return false;
    }
    for (int i = 0; i < 4; i++) {
        ret[i] = out.r[i];
    }
    return true;
}
