/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <platform/context.hpp>
#include <platform/types.hpp>

typedef uint64 Vcpu_id;
typedef uint64 Pcpu_id;
class Reg_accessor;

static constexpr Vcpu_id INVALID_VCPU_ID = ~0x0ull;

struct Vcpu_ctx {
    const Platform_ctx* ctx;
    Reg_accessor* regs;
    Vcpu_id vcpu_id;
};
