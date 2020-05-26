/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#pragma once

#include <platform/context.hpp>
#include <platform/types.hpp>
#include <platform/vm_types.hpp>

typedef uint64 Vcpu_id;
typedef uint64 Pcpu_id;

static constexpr Vcpu_id INVALID_VCPU_ID = ~0x0ull;

struct Vcpu_ctx {
    const Platform_ctx *ctx;
    const Reg_selection mtd_in;
    Reg_selection mtd_out;
    Vcpu_id vcpu_id;
};