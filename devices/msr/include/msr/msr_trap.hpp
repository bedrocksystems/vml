/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

#include <msr/msr.hpp>
#include <msr/msr_trap.hpp>

namespace vmi {

    class Wtrapped_msr : public Msr::Register_base {
    public:
        using Msr::Register_base::Register_base;
        virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vcpu, mword, uint8,
                                 uint64& res) override;
        virtual void reset() override;
        uint64 value();

    private:
        uint64 current{0};
    };

    class Sctlr_el1 : public Msr::Register_base {
    public:
        Sctlr_el1(const char* name, Msr::Id reg_id, Vbus::Bus& vbus)
            : Msr::Register_base(name, reg_id), _vbus(&vbus) {}

        virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vcpu, mword, uint8,
                                 uint64& res) override;
        virtual void reset() override;
        uint64 value();

    private:
        Vbus::Bus* _vbus;
        uint64 current{0};
    };

    enum Trapped_msr {
        SCTLR_EL1 = Msr::build_msr_id(3, 1, 0, 0, 0),
        TTBR0_EL1 = Msr::build_msr_id(3, 2, 0, 0, 0),
        TTBR1_EL1 = Msr::build_msr_id(3, 2, 0, 0, 1),
        TCR_EL1 = Msr::build_msr_id(3, 2, 0, 0, 2),
        AFSR0_EL1 = Msr::build_msr_id(3, 5, 0, 1, 0),
        AFSR1_EL1 = Msr::build_msr_id(3, 5, 0, 1, 1),
        ESR_EL1 = Msr::build_msr_id(3, 5, 0, 2, 0),
        FAR_EL1 = Msr::build_msr_id(3, 6, 0, 0, 0),
        MAIR_EL1 = Msr::build_msr_id(3, 0xa, 0, 2, 0),
        AMAIR_EL1 = Msr::build_msr_id(3, 0xa, 0, 3, 0),
        CONTEXTIDR_EL1 = Msr::build_msr_id(3, 0xd, 0, 3, 1),
    };

    bool setup_trapped_msr(Msr::Bus& bus, Vbus::Bus& vbus);

}
