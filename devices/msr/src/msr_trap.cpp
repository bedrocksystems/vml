/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <model/cpu.hpp>
#include <model/simple_as.hpp>
#include <msr/msr.hpp>
#include <msr/msr_info.hpp>
#include <msr/msr_trap.hpp>
#include <outpost/outpost.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <vmi_interface/vmi_interface.hpp>

static Vmm::Msr::Trap_id
convert_id_for_vmi(uint32 id) {
    switch (id) {
    case (vmi::TTBR0_EL1):
        return Vmm::Msr::TTBR0_EL1;
    case (vmi::TTBR1_EL1):
        return Vmm::Msr::TTBR1_EL1;
    case (vmi::TCR_EL1):
        return Vmm::Msr::TCR_EL1;
    default:
        return Vmm::Msr::UNKNOWN;
    }
}

Vbus::Err
vmi::Wtrapped_msr::access(Vbus::Access access, const Vcpu_ctx* vcpu, mword, uint8, uint64& res) {
    Vmm::Msr::Trap_info info;

    ASSERT(access == Vbus::Access::WRITE); // We only trap writes at the moment

    info.id = convert_id_for_vmi(id());
    if (info.id != Vmm::Msr::UNKNOWN) {
        info.read = false; // Only writes are trapped at the moment
        info.cur_value = current;
        info.new_value = res;
        info.name = name();
        outpost::vmi_handle_msr_update(*vcpu, info);
    }

    current = res;

    return Vbus::Err::UPDATE_REGISTER; // Tell the VCPU to update the relevant physical
                                       // register
}

void
vmi::Wtrapped_msr::reset() {
    current = 0;
}

uint64
vmi::Wtrapped_msr::value() {
    return current;
}

Vbus::Err
vmi::Sctlr_el1::access(Vbus::Access access, const Vcpu_ctx* vcpu, mword, uint8, uint64& res) {
    ASSERT(access == Vbus::Access::WRITE); // We only trap writes at the moment
    Reg_accessor regs(*vcpu->ctx, vcpu->mtd_in);
    Msr::Info::Sctlr_el1 before(regs.el1_sctlr()), after(res);

    current = res;

    /*
     * This is the counter-part of the Set/Way flushing logic emulation. Every time the
     * cache is toggled, we flush the guest AS. Moreover, if the cache is enabled we stop
     * trapping the virtual memory registers and wait for an eventual new (nothrow) call to Set/way
     * instructions before flushing again.
     *
     * Note that for now, VMI is not interested in that event so we simply don't forward it.
     */

    if (before.cache_enabled() != after.cache_enabled()) {
        DEBUG("Cache setting toggled - flushing the guest AS");

        _vbus->iter_devices(Model::Simple_as::flush_callback, nullptr);
    }

    if (after.cache_enabled()) {
        DEBUG("Cache enabled - stop TVM trapping");
        Model::Cpu::ctrl_tvm(vcpu->vcpu_id, false); // Turn off TVM
    }

    return Vbus::Err::UPDATE_REGISTER; // Tell the VCPU to update the relevant physical
                                       // register
}

void
vmi::Sctlr_el1::reset() {
    current = 0;
}

uint64
vmi::Sctlr_el1::value() {
    return current;
}

bool
vmi::setup_trapped_msr(Msr::Bus& bus, Vbus::Bus& vbus) {
    bool ok = false;

    ok = bus.register_system_reg(new (nothrow) Sctlr_el1("SCTLR_EL1", Msr::Id(SCTLR_EL1), vbus));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("TCR_EL1", Msr::Id(TCR_EL1)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("TTBR0_EL1", Msr::Id(TTBR0_EL1)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("TTBR1_EL1", Msr::Id(TTBR1_EL1)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("AFSR0_EL1", Msr::Id(AFSR0_EL1)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("AFSR1_EL1", Msr::Id(AFSR1_EL1)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("ESR_EL1", Msr::Id(ESR_EL1)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("FAR_EL1", Msr::Id(FAR_EL1)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("MAIR_EL1", Msr::Id(MAIR_EL1)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("MAIR1_A32", Msr::Id(Msr::MAIR1_A32)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("AMAIR_EL1", Msr::Id(AMAIR_EL1)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("DACR", Msr::Id(Msr::DACR)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow) Wtrapped_msr("IFSR", Msr::Id(Msr::IFSR)));
    if (!ok)
        return false;
    ok = bus.register_system_reg(new (nothrow)
                                     Wtrapped_msr("CONTEXTDIR_EL1", Msr::Id(CONTEXTIDR_EL1)));
    if (!ok)
        return false;

    return true;
}
