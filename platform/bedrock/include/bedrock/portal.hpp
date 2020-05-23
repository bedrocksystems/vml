/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <errno.hpp>
#include <nova/types.hpp>
#include <zeta/ec.hpp>
#include <zeta/types.hpp>

namespace Vcpu {
    class Vcpu;
};

namespace Portal {
    static constexpr Nova::Mtd MTD_CPU_STARTUP_INFO
        = Nova::MTD::GPR | Nova::MTD::EL1_SP | Nova::MTD::EL1_IDR | Nova::MTD::EL1_ELR_SPSR
          | Nova::MTD::EL1_ESR_FAR | Nova::MTD::EL1_AFSR | Nova::MTD::EL1_TTBR | Nova::MTD::EL1_TCR
          | Nova::MTD::EL1_MAIR | Nova::MTD::EL1_VBAR | Nova::MTD::EL1_SCTLR;

    static constexpr Nova::Mtd MTD_MSR_COMMON = Nova::MTD::EL2_ESR_FAR | Nova::MTD::GPR
                                                | Nova::MTD::EL2_ELR_SPSR | Nova::MTD::GIC
                                                | Nova::MTD::TMR | Nova::MTD::EL2_HCR;
    static constexpr Nova::Mtd MTD_MSR_TRAP_VM
        = Nova::MTD::EL1_TTBR | Nova::MTD::EL1_AFSR | Nova::MTD::EL1_MAIR | Nova::MTD::EL1_IDR
          | Nova::MTD::A32_DACR_IFSR | Nova::MTD::EL1_SCTLR | Nova::MTD::EL1_TCR;

    Errno init_portals(Zeta::Local_ec& lec, Sel exc_base_sel, Vcpu::Vcpu& vcpu);
    Errno ctrl_portal(Sel base, Sel id, Vcpu::Vcpu& vcpu);
    void add_regs(Sel id, Nova::Mtd mtd);
    void remove_regs(Sel id, Nova::Mtd mtd);
    void clear_regs(Sel id);
};