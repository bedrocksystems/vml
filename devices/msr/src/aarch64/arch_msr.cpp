/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/gic.hpp>
#include <model/simple_as.hpp>
#include <msr/arch_msr.hpp>
#include <msr/msr_info.hpp>

void
Msr::flush_on_cache_toggle(const VcpuCtx *vcpu, uint64 new_value) {
    if (!Model::Cpu::is_feature_enabled_on_vcpu(Model::Cpu::requested_feature_tvm, vcpu->vcpu_id)) {
        // Another requestor needed TVM - no action to take on our side
        return;
    }

    Msr::Info::SctlrEl1 before(vcpu->regs->el1_sctlr()), after(new_value);

    /*
     * This is the counter-part of the Set/Way flushing logic emulation. Every time the
     * cache is toggled, we flush the guest AS. Moreover, if the cache is enabled we stop
     * trapping the virtual memory registers and wait for an eventual new (nothrow) call to Set/way
     * instructions before flushing again.
     *
     * Note that for now, VMI is not interested in that event so we simply don't forward it.
     */

    if (before.cache_enabled() != after.cache_enabled()) {
        INFO("Cache setting toggled - flushing the guest AS");

        Vbus::Bus *bus = Msr::Set_way_flush_reg::get_associated_bus();
        ASSERT(bus != nullptr);
        bus->iter_devices<const VcpuCtx>(Model::SimpleAS::flush_callback, nullptr);
    }

    if (after.cache_enabled()) {
        INFO("Cache enabled - stop TVM trapping");
        Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_tvm, vcpu->vcpu_id, false);
    }
}

Msr::Err
Msr::SctlrEl1::access(Vbus::Access access, const VcpuCtx *vcpu, uint64 &res) {
    ASSERT(access == Vbus::Access::WRITE); // We only trap writes at the moment

    flush_on_cache_toggle(vcpu, res);
    return Msr::Err::UPDATE_REGISTER; // Tell the VCPU to update the relevant physical
                                      // register
}

bool
Msr::Bus::setup_aarch64_physical_timer(Model::AA64Timer &ptimer) {
    Msr::Register *reg;

    reg = new (nothrow) Msr::CntpTval("CNTP_TVAL_EL0", Msr::RegisterId::CNTP_TVAL_EL0, ptimer);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::CntpCtl("CNTP_CTL_EL0", Msr::RegisterId::CNTP_CTL_EL0, ptimer);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::CntpCval("CNTP_CVAL_EL0", Msr::RegisterId::CNTP_CVAL_EL0, ptimer);
    if (!register_system_reg(reg))
        return false;

    Msr::CntpctEl0 *cntpct = new (nothrow) Msr::CntpctEl0();
    return register_system_reg(cntpct);
}
