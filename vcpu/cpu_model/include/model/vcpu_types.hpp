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
class RegAccessor;

static constexpr Vcpu_id INVALID_VCPU_ID = ~0x0ull;

enum class CtxInfo {
    VMEXIT = 0, /*!< Default, we are handling a vmexit */
    TRANSLATE,  /*!< VCPU is performing a software page table walk */
    EMULATE,    /*!< VCPU is currently emulating instructions */
};

struct VcpuCtx {
    const Platform_ctx* ctx;
    RegAccessor* const regs;
    const Vcpu_id vcpu_id;
    CtxInfo info{CtxInfo::VMEXIT};

    VcpuCtx(const Platform_ctx* ctxv, RegAccessor* regsv, Vcpu_id vcpu_idv)
        : ctx(ctxv), regs(regsv), vcpu_id(vcpu_idv) {}

    VcpuCtx(VcpuCtx&&) = delete;
    VcpuCtx(const VcpuCtx&) = delete;
    VcpuCtx& operator=(VcpuCtx&&) = delete;
    VcpuCtx& operator=(const VcpuCtx&) = delete;
};

class CtxInfoGuard {
public:
    CtxInfoGuard(VcpuCtx& ctx, CtxInfo newctx) : _vctx(&ctx), _prev_info(ctx.info) {
        _vctx->info = newctx;
    }

    ~CtxInfoGuard() { _vctx->info = _prev_info; }

private:
    VcpuCtx* _vctx;
    const CtxInfo _prev_info;
};
