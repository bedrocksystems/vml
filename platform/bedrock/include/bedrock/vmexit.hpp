/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <nova/types.hpp>
#include <zeta/types.hpp>

namespace Vcpu {
    class Vcpu;
};

namespace Vmexit {
    Nova::Mtd startup(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd wfie(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd data_abort(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd instruction_abort(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd vtimer(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd recall(const Zeta::Zeta_ctx*, Vcpu::Vcpu&, const Nova::Mtd mtd);
    Nova::Mtd smc(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd msr(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd mrc_coproc1111(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd mrc_coproc1110(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd vmrs(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd brk(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd bkpt(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
    Nova::Mtd single_step(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd);
};