/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

#include <alloc/vmap.hpp>
#include <log/log.hpp>
#include <zeta/zeta.hpp>

static Errno
import_resource(const Zeta::Zeta_ctx *ctx, const Uuid &uuid, bool to_guest, mword &dst_va,
                mword &res_size) {
    Zeta::API::Uuid_info info;
    size_t len = 1;
    Errno err;

    err = Zeta::get_info(ctx, nullptr, uuid, &info, len);
    if (err != Errno::ENONE)
        return err;

    res_size = info.size;

    dst_va = reinterpret_cast<mword>(Vmap::pagealloc(align_up(res_size, PAGE_SIZE) / PAGE_SIZE));
    ASSERT(dst_va != 0);
    Nova::Crd crd = Nova::Crd(dst_va, 0, info.cred);
    err = Zeta::import(ctx, nullptr, uuid, crd, res_size, !to_guest, to_guest, false);

    return err;
}
