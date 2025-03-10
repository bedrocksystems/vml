/**
 * Copyright (C) 2020-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#pragma once

#include <platform/types.hpp>

typedef uint64 Vcpu_id;
typedef uint64 Pcpu_id;
typedef uint64 VcpuVHWId;
class RegAccessor;

static constexpr Vcpu_id INVALID_VCPU_ID = ~0x0ull;

enum class CtxInfo {
    VMEXIT = 0,  /*!< Default, we are handling a vmexit */
    TRANSLATE,   /*!< VCPU is performing a software page table walk */
    EMULATE,     /*!< VCPU is currently emulating instructions */
    COMMIT_INST, /*!< VCPU is committing instructions */
};

struct VcpuCtx {
    RegAccessor* const regs;
    const Vcpu_id vcpu_id;
    CtxInfo info{CtxInfo::VMEXIT};

    VcpuCtx(RegAccessor* regsv, Vcpu_id vcpu_idv) : regs(regsv), vcpu_id(vcpu_idv) {}

    VcpuCtx(VcpuCtx&&) = delete;
    VcpuCtx(const VcpuCtx&) = delete;
    VcpuCtx& operator=(VcpuCtx&&) = delete;
    VcpuCtx& operator=(const VcpuCtx&) = delete;
};

class CtxInfoGuard {
public:
    CtxInfoGuard(VcpuCtx& ctx, CtxInfo newctx) : _vctx(&ctx), _prev_info(ctx.info) { _vctx->info = newctx; }

    ~CtxInfoGuard() { _vctx->info = _prev_info; }

private:
    VcpuCtx* _vctx;
    const CtxInfo _prev_info;
};
