/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bedrock/portal.hpp>
#include <bedrock/vmexit.hpp>
#include <debug_switches.hpp>
#include <log/log.hpp>
#include <model/board.hpp>
#include <model/cpu.hpp>
#include <model/platform_firmware.hpp>
#include <model/psci.hpp>
#include <msr/esr.hpp>
#include <msr/msr.hpp>
#include <msr/msr_info.hpp>
#include <msr/msr_trap.hpp>
#include <nova/types.hpp>
#include <outpost/outpost.hpp>
#include <platform/reg_accessor.hpp>
#include <vcpu/vcpu.hpp>
#include <vcpu/vcpu_roundup.hpp>
#include <zeta/types.hpp>

/*
 * Here is the current contract with NOVA:
 * X0 contains id_aa64pfr0_el1
 * X1 contains id_aa64pfr1_el1
 * X2 contains id_aa64dfr0_el1
 * X3 contains id_aa64dfr1_el1
 * X4 contains id_aa64isar0_el1
 * X5 contains id_aa64isar1_el1
 * X6 contains id_aa64mmfr0_el1
 * X7 contains id_aa64mmfr1_el1
 * X8 contains id_aa64mmfr2_el1
 * X9 contains id_aa64zfr0_el1
 * X16 contains id_pfr1_el1 (high 32 bits) and id_pfr0_el1 (low 32 bits)
 * X17 contains id_dfr0_el1 (high 32 bits) and id_pfr2_el1 (low 32 bits)
 * X18 contains id_isar0_el1 (high 32 bits) and id_dfr1_el1 (low 32 bits)
 * X19 contains id_isar2_el1 (high 32 bits) and id_isar1_el1 (low 32 bits)
 * X20 contains id_isar4_el1 (high 32 bits) and id_isar3_el1 (low 32 bits)
 * X21 contains id_isar6_el1 (high 32 bits) and id_isar5_el1 (low 32 bits)
 * X22 contains id_mmfr1_el1 (high 32 bits) and id_mmfr0_el1 (low 32 bits)
 * X23 contains id_mmfr3_el1 (high 32 bits) and id_mmfr2_el1 (low 32 bits)
 * X24 contains id_mmfr5_el1 (high 32 bits) and id_mmfr4_el1 (low 32 bits)
 * X29 contains mvfr1_el1 (high 32 bits) and mvfr0_el1 (low 32 bits)
 * X30 contains mvfr2_el1 (low 32 bits)
 * EL1_SP contains ccsidr_el1 corresponding to csselr_el1 {Level=1, InD=0}
 * EL1_TPIDR contains ccsidr_el1 corresponding to csselr_el1 {Level=1, InD=1}
 * EL1_CONTEXTIDR/EL1_ELR likewise for {Level=2}
 * EL1_SPSR/EL1_ESR likewise for {Level=3}
 * EL1_FAR/EL1_AFSR0 likewise for {Level=4}
 * EL1_AFSR1/EL1_TTBR0 likewise for {Level=5}
 * EL1_TTBR1/EL1_TCR likewise for {Level=6}
 * EL1_MAIR/EL1_AMAIR likewise for {Level=7}
 * EL1_VBAR contains ctr_el0
 * EL1_SCTLR contains clidr_el1
 */
static void
prepare_msr_info(const Reg_accessor& arch, Msr::Bus::Platform_info& info) {
    info.id_aa64pfr0_el1 = arch.gpr(0);
    info.id_aa64pfr1_el1 = arch.gpr(1);
    info.id_aa64dfr0_el1 = arch.gpr(2);
    info.id_aa64dfr1_el1 = arch.gpr(3);
    info.id_aa64isar0_el1 = arch.gpr(4);
    info.id_aa64isar1_el1 = arch.gpr(5);
    info.id_aa64mmfr0_el1 = arch.gpr(6);
    info.id_aa64mmfr1_el1 = arch.gpr(7);
    info.id_aa64mmfr2_el1 = arch.gpr(8);
    info.id_aa64zfr0_el1 = arch.gpr(9);

    info.id_pfr0_el1 = uint32(arch.gpr(16));
    info.id_pfr1_el1 = uint32(arch.gpr(16) >> 32);
    info.id_pfr2_el1 = uint32(arch.gpr(17));
    info.id_isar0_el1 = uint32(arch.gpr(18) >> 32);
    info.id_isar1_el1 = uint32(arch.gpr(19));
    info.id_isar2_el1 = uint32(arch.gpr(19) >> 32);
    info.id_isar3_el1 = uint32(arch.gpr(20));
    info.id_isar4_el1 = uint32(arch.gpr(20) >> 32);
    info.id_isar5_el1 = uint32(arch.gpr(21));
    info.id_isar6_el1 = uint32(arch.gpr(21) >> 32);

    info.id_mmfr0_el1 = uint32(arch.gpr(22));
    info.id_mmfr1_el1 = uint32(arch.gpr(22) >> 32);
    info.id_mmfr2_el1 = uint32(arch.gpr(23));
    info.id_mmfr3_el1 = uint32(arch.gpr(23) >> 32);
    info.id_mmfr4_el1 = uint32(arch.gpr(24));
    info.id_mmfr5_el1 = uint32(arch.gpr(24) >> 32);

    info.mvfr0_el1 = uint32(arch.gpr(29));
    info.mvfr1_el1 = uint32(arch.gpr(29) >> 32);
    info.mvfr2_el1 = uint32(arch.gpr(30));

    info.midr_el1 = arch.el2_vpidr();
    info.id_dfr0_el1 = uint32(arch.gpr(17) >> 32);
    info.id_dfr1_el1 = 0;

    info.clidr_el1 = arch.el1_sctlr();
    info.ctr_el0 = arch.el1_vbar();
    info.ccsidr_el1[0] = arch.el1_sp();
    info.ccsidr_el1[1] = arch.el1_tpidr();
    info.ccsidr_el1[2] = arch.el1_contextidr();
    info.ccsidr_el1[3] = arch.el1_elr();
    info.ccsidr_el1[4] = arch.el1_spsr();
    info.ccsidr_el1[5] = arch.el1_esr();
    info.ccsidr_el1[6] = arch.el1_far();
    info.ccsidr_el1[7] = arch.el1_afsr0();
    info.ccsidr_el1[8] = arch.el1_afsr1();
    info.ccsidr_el1[9] = arch.el1_ttbr0();
    info.ccsidr_el1[10] = arch.el1_ttbr1();
    info.ccsidr_el1[11] = arch.el1_tcr();
    info.ccsidr_el1[12] = arch.el1_mair();
    info.ccsidr_el1[13] = arch.el1_amair();
}

Nova::Mtd
Vmexit::startup(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in) {
    Reg_accessor arch(*ctx, mtd_in);

    INFO("Affinity mapping from PCPU %u-%u-%u-%u to VCPU %u-%u-%u-%u",
         (arch.el2_vmpidr() >> 32) & 0xff, (arch.el2_vmpidr() >> 16) & 0xff,
         (arch.el2_vmpidr() >> 8) & 0xff, (arch.el2_vmpidr()) & 0xff, vcpu.aff3(), vcpu.aff2(),
         vcpu.aff1(), vcpu.aff0());

    Msr::Bus::Platform_info info;
    prepare_msr_info(arch, info);
    bool ok = vcpu.msr_bus.setup_arch_msr(info, *(vcpu.board->get_bus()), *(vcpu.board->get_gic()));
    if (!ok)
        ABORT_WITH("Unable to setup the MSR bus");

    ok = vcpu.msr_bus.setup_aarch64_physical_timer(vcpu.ptimer);
    if (!ok)
        ABORT_WITH("Unable to hook the physical timer to the MSR bus");

    Msr::Info::Id_aa64pfr0 aa64pfr0(arch.gpr(0));

    if (aa64pfr0.get_supported_mode(Msr::Info::Id_aa64pfr0::EL1_SHIFT)
        == Msr::Info::Id_aa64pfr0::AA64_ONLY) {
        ABORT_WITH("AArch32 requested but is not supported by the platform. AA64PFR0 EL1: 0x%x",
                   aa64pfr0.get_supported_mode(Msr::Info::Id_aa64pfr0::EL1_SHIFT));
    }

    if (Debug::TRACE_SYSTEM_REGS)
        vcpu.msr_bus.set_trace(true, true);
    if (Debug::TRACE_VBUS)
        vcpu.board->get_bus()->set_trace(true, true);

    Model::Cpu::reconfigure(vcpu.id(), Model::Cpu::VCPU_RECONFIG_SWITCH_OFF);
    Model::Cpu::reconfigure(vcpu.id(), Model::Cpu::VCPU_RECONFIG_RESET);

    ok = vmi::setup_trapped_msr(vcpu.msr_bus, *(vcpu.board->get_bus()));
    if (!ok)
        ABORT_WITH("Unable to configure register trapping for VMI");

    Vcpu_ctx vctx{ctx, mtd_in, 0, vcpu.id()};
    outpost::vmi_vcpu_startup(vctx);

    return arch.get_reg_selection_out();
}

Nova::Mtd
Vmexit::wfie(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in) {
    Reg_accessor arch(*ctx, mtd_in);

    arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR);

    if (!(arch.el2_esr() & 1))
        vcpu.wait_for_interrupt(arch.tmr_cntv_ctl(), arch.tmr_cntv_cval() + arch.tmr_cntvoff());

    arch.advance_pc();
    return arch.get_reg_selection_out();
}

static void
system_register_update_reg(Vcpu::Vcpu& vcpu, Reg_accessor& arch, Msr::Access const& msr_info,
                           uint64 reg_value) {
    if (Model::Cpu::is_tvm_enabled(vcpu.id()) && msr_info.write()) {
        switch (msr_info.id()) {
        case (vmi::SCTLR_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_SCTLR);
            arch.el1_sctlr(reg_value);
            break;
        }
        case (vmi::TTBR0_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_TTBR);
            arch.el1_ttbr0(reg_value);
            break;
        }
        case (vmi::TTBR1_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_TTBR);
            arch.el1_ttbr1(reg_value);
            break;
        }
        case (vmi::TCR_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_TCR);
            arch.el1_tcr(reg_value);
            break;
        }
        case (vmi::AFSR0_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_AFSR);
            arch.el1_afsr0(reg_value);
            break;
        }
        case (vmi::AFSR1_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_AFSR);
            arch.el1_afsr1(reg_value);
            break;
        }
        case (vmi::ESR_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_ESR_FAR);
            arch.el1_esr(reg_value);
            break;
        }
        case (vmi::FAR_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_ESR_FAR);
            arch.el1_far(reg_value);
            break;
        }
        case (vmi::AMAIR_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_MAIR);
            arch.el1_amair(reg_value);
            break;
        }
        case (vmi::CONTEXTIDR_EL1): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_IDR);
            arch.el1_contextidr(reg_value);
            break;
        }
        case (vmi::MAIR_EL1): {
            Msr::Info::Spsr spsr(arch.el2_spsr());

            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_MAIR);
            if (spsr.is_aa32()) {
                arch.el1_mair((arch.el1_mair() & (~0xffffffffull)) | (reg_value & 0xffffffffull));
            } else {
                arch.el1_mair(reg_value);
            }
            break;
        }
        case (Msr::MAIR1_A32): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL1_MAIR);
            arch.el1_mair(((reg_value << 32) & (~0xffffffffull))
                          | (arch.el1_mair() & 0xffffffffull));
            break;
        }
        case (Msr::DACR): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::A32_DACR_IFSR);
            arch.a32_dacr(static_cast<uint32>(reg_value));
            break;
        }
        case (Msr::IFSR): {
            arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::A32_DACR_IFSR);
            arch.a32_ifsr(static_cast<uint32>(reg_value));
            break;
        }
        default:
            ABORT_WITH("unknown trapped msr rt=%x operation=%s id=%u reg_value=%llx",
                       msr_info.target_reg(), msr_info.write() ? "write" : "read", msr_info.id(),
                       reg_value);
        }
    } else {
        arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::GPR);
        arch.gpr(msr_info.target_reg(), reg_value);
    }
}

static Nova::Mtd
system_register(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in,
                const Msr::Access& msr_info) {
    Reg_accessor arch(*ctx, mtd_in);
    uint64 reg_value = arch.gpr(msr_info.target_reg());
    Vbus::Err err;
    Vcpu_ctx vcpu_ctx{ctx, mtd_in, 0, vcpu.id()};

    err = vcpu.handle_msr_exit(&vcpu_ctx, msr_info, reg_value);
    switch (err) {
    case (Vbus::Err::UPDATE_REGISTER): {
        system_register_update_reg(vcpu, arch, msr_info, reg_value);
        arch.advance_pc();
        break;
    }
    case (Vbus::Err::OK): {
        arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR);
        arch.advance_pc();
        break;
    }
    default:
        ABORT_WITH(
            "unknown system register esr=%lx rt=%x operation=%s id=%u rt_value=%llx elr=%llx",
            ctx->utcb()->arch.el2_esr, msr_info.target_reg(), msr_info.write() ? "write" : "read",
            msr_info.id(), reg_value, ctx->utcb()->arch.el2_elr);
    }

    /*
     * Allow handle_msr_exit to modify the state of the registers on that VCPU.
     * XXX: It is assumed that handle_msr_exit knows what its doing. In particular,
     * VMI could modify states here.
     */
    return arch.get_reg_selection_out() | vcpu_ctx.mtd_out;
}

bool
verify_aarch32_condition(const Esr::Mcr_mrc::Cond cond, const uint64 el2_spsr) {
    const Msr::Info::Spsr spsr(el2_spsr);

    switch (cond) {
    case Esr::Mcr_mrc::Cond::COND_EQ:
        return spsr.is_Z();
    case Esr::Mcr_mrc::Cond::COND_NE:
        return !spsr.is_Z();
    case Esr::Mcr_mrc::Cond::COND_CS:
        return spsr.is_C();
    case Esr::Mcr_mrc::Cond::COND_CC:
        return !spsr.is_C();
    case Esr::Mcr_mrc::Cond::COND_MI:
        return spsr.is_N();
    case Esr::Mcr_mrc::Cond::COND_PL:
        return !spsr.is_N();
    case Esr::Mcr_mrc::Cond::COND_VS:
        return spsr.is_V();
    case Esr::Mcr_mrc::Cond::COND_VC:
        return !spsr.is_V();
    case Esr::Mcr_mrc::Cond::COND_HI:
        return spsr.is_C() && !spsr.is_Z();
    case Esr::Mcr_mrc::Cond::COND_LS:
        return !spsr.is_C() || spsr.is_Z();
    case Esr::Mcr_mrc::Cond::COND_GE:
        return spsr.is_N() == spsr.is_V();
    case Esr::Mcr_mrc::Cond::COND_LT:
        return spsr.is_N() != spsr.is_V();
    case Esr::Mcr_mrc::Cond::COND_GT:
        return !spsr.is_Z() && (spsr.is_N() == spsr.is_V());
    case Esr::Mcr_mrc::Cond::COND_LE:
        return spsr.is_Z() || (spsr.is_N() != spsr.is_V());
    case Esr::Mcr_mrc::Cond::COND_AL:
        return true;
    default:
        /*
         * The spec says that some unconditional instructions can have a value of 0b1111.
         * Probably, in that case CV will be false and we won't come in here. But, let's
         * be paranoid.
         */
        WARN("Possibly malformed condition when emulating MCR/MRC access: %u", cond);
        return true;
    }
}

Msr::Access
convert_msr_id_to_a64(uint8 const coproc, Esr::Mcr_mrc const esr) {
    Msr::Access acc(coproc, esr.crn(), esr.opc1(), esr.crm(), esr.opc2(), esr.rt(), esr.write());

    switch (acc.id()) {
    case Msr::Register_id::CTR_A32:
        return Msr::Access(Msr::Register_id::CTR_A64, acc.target_reg(), acc.write());
    case Msr::Register_id::DCISW_A32:
        return Msr::Access(Msr::Register_id::DCISW_A64, acc.target_reg(), acc.write());
    case Msr::Register_id::DCCSW_A32:
        return Msr::Access(Msr::Register_id::DCCSW_A64, acc.target_reg(), acc.write());
    case Msr::Register_id::DCCISW_A32:
        return Msr::Access(Msr::Register_id::DCCISW_A64, acc.target_reg(), acc.write());
    case Msr::Register_id::DACR_A32:
        return Msr::Access(Msr::Register_id::DACR, acc.target_reg(), acc.write());
    case Msr::Register_id::IFSR_A32:
        return Msr::Access(Msr::Register_id::IFSR, acc.target_reg(), acc.write());
    case Msr::Register_id::CONTEXTIDR_A32:
        return Msr::Access(Msr::Register_id::CONTEXTIDR_EL1, acc.target_reg(), acc.write());

    default:
        /*
         * Most registers can be directly translated into their 64-bit equivalent. The exceptions
         * are list above.
         */
        return Msr::Access(coproc & 0x3, esr.crn(), esr.opc1(), esr.crm(), esr.opc2(), esr.rt(),
                           esr.write());
    }
}

static Msr::Access
convert_vmrs_reg_to_msr(Esr::Mcr_mrc const esr) {
    switch (esr.crn()) {
    case (Msr::Info::VMRS_SPEC_REG_FPSID):
        return Msr::Access(Msr::Register_id::FPSID, esr.rt(), esr.write());
    case (Msr::Info::VMRS_SPEC_REG_MVFR0):
        return Msr::Access(Msr::Register_id::MVFR0, esr.rt(), esr.write());
    case (Msr::Info::VMRS_SPEC_REG_MVFR1):
        return Msr::Access(Msr::Register_id::MVFR1, esr.rt(), esr.write());
    case (Msr::Info::VMRS_SPEC_REG_MVFR2):
        return Msr::Access(Msr::Register_id::MVFR2, esr.rt(), esr.write());
    default:
        ABORT_WITH("Unrecognized spec_reg for VMRS access: %u", esr.crn());
        return Msr::Access(Msr::Register_id::INVALID_ID, 0, 0);
    }
}

Nova::Mtd
Vmexit::vmrs(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in) {
    Reg_accessor arch(*ctx, mtd_in);
    Esr::Mcr_mrc const esr(arch.el2_esr());

    if (esr.cv() && !verify_aarch32_condition(esr.cond(), arch.el2_spsr())) {
        DEBUG("VMRS @ 0x%llx didn't meet its condition - skipping", ctx->utcb()->arch.el2_elr);
        arch.advance_pc();
        return arch.get_reg_selection_out();
    }

    return system_register(ctx, vcpu, mtd_in, convert_vmrs_reg_to_msr(esr));
}

static Nova::Mtd
mrc(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in, uint8 coproc) {
    Reg_accessor arch(*ctx, mtd_in);
    Esr::Mcr_mrc const esr(arch.el2_esr());

    if (esr.cv() && !verify_aarch32_condition(esr.cond(), arch.el2_spsr())) {
        DEBUG("MCR/MRC @ 0x%llx didn't meet its condition - skipping", arch.el2_elr());
        arch.advance_pc();
        return arch.get_reg_selection_out();
    }

    return system_register(ctx, vcpu, mtd_in, convert_msr_id_to_a64(coproc, esr));
}

Nova::Mtd
Vmexit::mrc_coproc1111(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd) {
    return mrc(ctx, vcpu, mtd, uint8(0b1111));
}

Nova::Mtd
Vmexit::mrc_coproc1110(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd) {
    return mrc(ctx, vcpu, mtd, uint8(0b1110));
}

Nova::Mtd
Vmexit::msr(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in) {
    Reg_accessor arch(*ctx, mtd_in);
    const Esr::Msr_mrs esr(arch.el2_esr());
    Msr::Access acc(esr.op0(), esr.crn(), esr.op1(), esr.crm(), esr.op2(), esr.rt(), esr.write());

    return system_register(ctx, vcpu, mtd_in, acc);
}

Nova::Mtd
Vmexit::data_abort(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in) {
    Reg_accessor arch(*ctx, mtd_in);
    Esr::Data_abort const esr(arch.el2_esr());
    uint64 const fault_paddr = ((arch.el2_hpfar() << 8) & ~0xfffull) | (arch.el2_far() & 0xfffull);
    uint64 reg_value = arch.gpr(esr.reg());

    Vcpu_ctx vcpu_ctx{ctx, mtd_in, 0, vcpu.id()};
    Vbus::Err err = vcpu.handle_data_abort(&vcpu_ctx, fault_paddr, esr, reg_value);
    ASSERT(vcpu_ctx.mtd_out == 0); // No modification allowed at the moment

    switch (err) {
    case Vbus::Err::UPDATE_REGISTER:
        arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::GPR);
        arch.gpr(esr.reg(), reg_value);
        arch.advance_pc();
        break;
    case Vbus::Err::OK:
        arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR);
        arch.advance_pc();
        break;
    case Vbus::Err::REPLAY_INST:
        break;
    case Vbus::Err::NO_DEVICE:
        ABORT_WITH("no device to handle data abort esr=0x%llx ip=0x%llx fault_paddr=0x%llx",
                   arch.el2_esr(), arch.el2_elr(), fault_paddr);
    case Vbus::Err::ACCESS_ERR: {
        const Model::Board* b = vcpu.board;
        const Vbus::Bus* bus = b->get_bus();
        const Vbus::Device* dev = bus->get_device_at(fault_paddr, 1);

        ABORT_WITH(
            "device '%s' unable to handle %s access @ fault_paddr=0x%llx:%u - esr=0x%llx ip=0x%llx",
            dev != nullptr ? dev->name() : nullptr, esr.write() ? "write" : "read", fault_paddr,
            esr.access_size_bytes(), arch.el2_esr(), arch.el2_elr());
    }
    default:
        ABORT_WITH("unknown error on data abort esr=0x%llx ip=0x%llx fault_paddr=0x%llx",
                   arch.el2_esr(), arch.el2_elr(), fault_paddr);
    }

    return arch.get_reg_selection_out();
}

Nova::Mtd
Vmexit::instruction_abort(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in) {
    Reg_accessor arch(*ctx, mtd_in);
    const Esr::Instruction_abort esr(arch.el2_esr());
    uint64 const fault_paddr = ((arch.el2_hpfar() << 8) & ~0xfffull) | (arch.el2_far() & 0xfffull);

    if (esr.sync_err_type() != Esr::Instruction_abort::RECOVERABLE || esr.far_not_valid()
        || esr.fault_type() == Esr::Instruction_abort::OTHER_FAULT) {
        ABORT_WITH("Cannot handle instruction abort esr=0x%llx ip=0x%llx fault_paddr=0x%llx",
                   arch.el2_esr(), arch.el2_elr(), fault_paddr);
    }

    Vcpu_ctx vcpu_ctx{ctx, mtd_in, 0, vcpu.id()};
    Vbus::Err err = vcpu.handle_instruction_abort(&vcpu_ctx, fault_paddr, esr);
    ASSERT(vcpu_ctx.mtd_out == 0); // No modification allowed at the moment

    switch (err) {
    case Vbus::Err::REPLAY_INST:
    case Vbus::Err::OK:
        // For now, we don't emulate instructions so OK == REPLAY_INST. This will change in the
        // future.
        break;
    case Vbus::Err::NO_DEVICE:
        ABORT_WITH("no device to handle instruction abort esr=0x%llx ip=0x%llx fault_paddr=0x%llx",
                   arch.el2_esr(), arch.el2_elr(), fault_paddr);
    case Vbus::Err::UPDATE_REGISTER:
    case Vbus::Err::ACCESS_ERR: {
        const Model::Board* b = vcpu.board;
        const Vbus::Bus* bus = b->get_bus();
        const Vbus::Device* dev = bus->get_device_at(fault_paddr, 1);

        ABORT_WITH("device '%s' unable to handle execution access @ fault_paddr=0x%llx:%u - "
                   "esr=0x%llx ip=0x%llx",
                   dev != nullptr ? dev->name() : nullptr, fault_paddr, esr.instruction_len_bytes(),
                   arch.el2_esr(), arch.el2_elr());
    }
    default:
        ABORT_WITH("unknown error on instruction abort esr=0x%llx ip=0x%llx fault_paddr=0x%llx",
                   arch.el2_esr(), arch.el2_elr(), fault_paddr);
    }

    return arch.get_reg_selection_out();
}

Nova::Mtd
Vmexit::vtimer(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in) {
    Reg_accessor arch(*ctx, mtd_in);

    vcpu.assert_vtimer(arch.tmr_cntv_ctl());
    return arch.get_reg_selection_out();
}

Nova::Mtd
Vmexit::recall(const Zeta::Zeta_ctx*, Vcpu::Vcpu&, const Nova::Mtd) {
    return 0;
}

Nova::Mtd
Vmexit::smc(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd_in) {
    Reg_accessor arch(*ctx, mtd_in);
    uint64 x0 = arch.gpr(0);
    uint64 x1 = arch.gpr(1);
    uint64 x2 = arch.gpr(2);
    uint64 x3 = arch.gpr(3);
    uint64 elr = arch.el2_elr();
    uint64 res = 0;

    arch.set_reg_selection_out(Nova::MTD::EL2_ELR_SPSR | Nova::MTD::GPR);

    if (Debug::TRACE_SMC)
        INFO("smc cpu %u - %llx %llx %llx %llx - elr %llx", vcpu.id(), x0, x1, x2, x3, elr);

    switch (x0 & 0x3fffffffu) {
    case 0x4000000 ... 0x400ffff: { // Service calls
        Vcpu_ctx vcpu_ctx{ctx, mtd_in, 0, vcpu.id()};
        Vbus::Bus* vbus = vcpu.board->get_bus();

        if (!Firmware::Psci::smc_call_service(vcpu_ctx, *vbus, x0, res)) {
            ABORT_WITH("unsupported Psci call %llx", x0);
            return arch.get_reg_selection_out();
        }
        ASSERT(vcpu_ctx.mtd_out == 0); // No modification allowed at the moment
        arch.gpr(0, res);
        break;
    }

    case 0x2000000 ... 0x200ffff: { // SIP
        Model::Firmware* fw = vcpu.board->get_firmware();

        if (fw == nullptr) {
            ABORT_WITH("Unsupported SIP call 0x%llx", x0);
            arch.gpr(0, -1ull);
            break;
        }

        Vcpu_ctx vcpu_ctx{ctx, mtd_in, 0, vcpu.id()};
        mword ret[4];
        bool handled = fw->handle_smc(&vcpu_ctx, arch.gpr(0), arch.gpr(1), arch.gpr(2), arch.gpr(3),
                                      arch.gpr(4), arch.gpr(5), arch.gpr(6), ret);
        ASSERT(vcpu_ctx.mtd_out == 0); // No modification allowed at the moment

        if (handled) {
            arch.gpr(0, ret[0]);
            arch.gpr(1, ret[1]);
            arch.gpr(2, ret[2]);
            arch.gpr(3, ret[3]);
        } else {
            ABORT_WITH("Unhandled SIP call 0x%llx", x0);
            arch.gpr(0, -1ull);
        }
        break;
    }

    default:
        ABORT_WITH("Unsupported SMC 0x%llx", x0);
        arch.gpr(0, -1ull);
        break;
    }

    arch.advance_pc();
    return arch.get_reg_selection_out();
}

Nova::Mtd
Vmexit::brk(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd) {
    /*
     * If the 'brk' is in the guest code and was added there by the guest, we can be in:
     * 1 - EL0 executed brk -> inject an exception from a lower EL with AA64
     * 2 - EL1 executed brk -> inject an exception from the same EL and check SPsel
     */
    Reg_accessor arch(*ctx, mtd);
    Msr::Info::Spsr spsr(arch.el2_spsr());
    Vcpu::Vcpu::Exception_class c;

    if (spsr.el() == Msr::Info::AA64_EL1)
        if (spsr.spx())
            c = Vcpu::Vcpu::SAME_EL_SPX;
        else
            c = Vcpu::Vcpu::SAME_EL_SP0;
    else
        c = Vcpu::Vcpu::LOWER_EL_AA64;

    return vcpu.forward_exception(*ctx, mtd, c, Vcpu::Vcpu::SYNC, false);
}

Nova::Mtd
Vmexit::bkpt(const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu, const Nova::Mtd mtd) {
    /*
     * If the 'bkpt' is in the guest code and was added there by the guest, we can be in:
     * 1 - EL0 executed bkpt, EL1 runs AA64 -> inject an exception from a lower EL with AA32
     * 2 - EL0 executed bkpt, EL1 runs AA32 -> inject a prefetch abort (AA32 style)
     * 3 - EL1 executed bkpt (so EL1 has to run AA32) -> inject a prefetch abort (AA32 style)
     */
    if (vcpu.aarch64()) {
        return vcpu.forward_exception(*ctx, mtd, Vcpu::Vcpu::LOWER_EL_AA32, Vcpu::Vcpu::SYNC,
                                      false);
    } else
        ABORT_WITH("BKPT unsupported with AA32 guests for now");
}