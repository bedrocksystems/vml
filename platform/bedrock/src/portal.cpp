/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <bedrock/portal.hpp>
#include <bedrock/vmexit.hpp>
#include <debug_switches.hpp>
#include <log/log.hpp>
#include <outpost/outpost.hpp>
#include <vcpu/vcpu.hpp>
#include <zeta/ec.hpp>
#include <zeta/types.hpp>
#include <zeta/zeta.hpp>

typedef Nova::Mtd (*vcpu_portal_handler)(const Zeta::Zeta_ctx*, Vcpu::Vcpu&, const Nova::Mtd);

static void
sanity_check_on_vmexit(const Sel vmexit_id, const Zeta::Zeta_ctx*, const Vcpu::Vcpu& vcpu,
                       const Nova::Mtd& mtd_in) {
    if ((mtd_in & Nova::MTD::GIC) == 0) {
        WARN("VCPU %u: VMExit: 0x%llx: GIC state was not requested from NOVA", vcpu.id(),
             vmexit_id);
    }
}

static void
sanity_check_before_vmresume(const Sel vmexit_id, const Zeta::Zeta_ctx* ctx, const Vcpu::Vcpu& vcpu,
                             const Nova::Mtd& mtd_out) {
    Nova::Utcb_arch& arch = ctx->utcb()->arch;

    if (arch.el2_elr == 0) {
        WARN("VCPU %u: VMExit: 0x%llx: EL2_ELR is set to zero.", vcpu.id(), vmexit_id);
    }

    if (vmexit_id != Nova::Exc::VCPU_RECALL && vmexit_id != Nova::Exc::VCPU_VTIMER) {
        if ((mtd_out & Nova::MTD::EL2_ELR_SPSR) == 0) {
            WARN("VCPU %u: VMExit: 0x%llx: EL2_ELR_SPSR was not set in the MTD", vcpu.id(),
                 vmexit_id);
        }
    }
}

template<vcpu_portal_handler H>
inline Nova::Mtd
call_portal_handler(const Sel vmexit_id, const Zeta::Zeta_ctx* ctx, Vcpu::Vcpu& vcpu,
                    const Nova::Mtd& mtd_in) {
    Nova::Mtd mtd_out = 0;

    if (Debug::SANITY_CHECK_VM_EXIT_RESUME)
        sanity_check_on_vmexit(vmexit_id, ctx, vcpu, mtd_in);

    do {
        if (vcpu.switch_state_to_emulating())
            break;
        DEBUG("VMI recall callback");

        Vcpu_ctx vctx{ctx, mtd_in, 0, vcpu.id()};
        outpost::vmi_handle_recall(vctx);
        vcpu.wait_for_resume();
    } while (true);
    // Emulation mode starts here

    mtd_out = vcpu.check_reset(*ctx, mtd_in);

    /*
     * If we are asked to reset, mtd_out will be non-zero. In that case, there is not point
     * in emulating the current VM exit. It will be irrelevant (or even wrong to emulate).
     */
    if (mtd_out == 0) {
        if (vmexit_id != Nova::Exc::VCPU_STARTUP) // No interrupt handling at startup
            mtd_out |= vcpu.update_inj_status(*ctx, mtd_in);

        mtd_out |= H(ctx, vcpu, mtd_in);

        if (vmexit_id != Nova::Exc::VCPU_STARTUP) // No interrupt handling at startup
            mtd_out |= vcpu.inject_irqs(*ctx, mtd_in);
    }

    mtd_out |= vcpu.reconfigure(*ctx, mtd_in);

    // Emulation mode stops here
    vcpu.switch_state_to_on();

    if (Debug::SANITY_CHECK_VM_EXIT_RESUME)
        sanity_check_before_vmresume(vmexit_id, ctx, vcpu, mtd_out);

    return mtd_out;
}

/*! \brief Configuration associated with each portal provided by NOVA
 */
struct Portal_entry_config {
    Nova::Mtd mtd;                //!< Registers needed by the VMM to operate
    zeta_portal_entry entry;      //!< Entry point for the portal handler
    atomic<Nova::Mtd> extra_regs; //!< Extra registers that can be reconfigured at run-time
};

#ifndef __clang__
#pragma GCC diagnostic push
/*
 * Certain portal handlers below do not return. However, adding the 'noreturn' attribute is not nice
 * because:
 * - It requires tricking the ZETA_PORTAL macro
 * - Even if we added the attribute, the compiler would complain that we are doing "return 0"
 * in a function that is not supposed to return.
 * - We could then remove the return but the compiler would warn that the function
 * should be return 'void' instead of 'mword'. To fix, we would need to define a new
 * ZETA_PORTAL macro... That's not really desired at this point. Let's just disable the warning
 * locally for now.
 */
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
#endif

ZETA_PORTAL(startup_handler, Vcpu::Vcpu& vcpu, mword mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::startup>(Nova::Exc::VCPU_STARTUP, ctx, vcpu, mtd);
}
EXPORT_PORTAL(startup_handler, mword);

ZETA_PORTAL(msr_mrs_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::msr>(Nova::Exc::MSR_MRS, ctx, vcpu, mtd);
}
EXPORT_PORTAL(msr_mrs_handler, mword);

ZETA_PORTAL(wfie_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::wfie>(Nova::Exc::WF_IE, ctx, vcpu, mtd);
}
EXPORT_PORTAL(wfie_handler, mword);

ZETA_PORTAL(recall_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::recall>(Nova::Exc::VCPU_RECALL, ctx, vcpu, mtd);
}
EXPORT_PORTAL(recall_handler, mword);

ZETA_PORTAL(vtimer_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::vtimer>(Nova::Exc::VCPU_VTIMER, ctx, vcpu, mtd);
}
EXPORT_PORTAL(vtimer_handler, mword);

ZETA_PORTAL(dabort_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::data_abort>(Nova::Exc::DABORT_EL, ctx, vcpu, mtd);
}
EXPORT_PORTAL(dabort_handler, mword);

ZETA_PORTAL(smc_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::smc>(Nova::Exc::SMC, ctx, vcpu, mtd);
}
EXPORT_PORTAL(smc_handler, mword);

ZETA_PORTAL(unknown_reason_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx*) {
    ABORT_WITH("Unsupported VM Exit: unknown reason");
    return 0;
}
EXPORT_PORTAL(unknown_reason_handler, mword);

ZETA_PORTAL(mcr_mrc_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::mrc_coproc1111>(0x3, ctx, vcpu, mtd);
}
EXPORT_PORTAL(mcr_mrc_handler, mword);

ZETA_PORTAL(mcrr_mrrc_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: MCRR/MRRC. ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(mcrr_mrrc_handler, mword);

ZETA_PORTAL(mcr_mrc_2_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::mrc_coproc1110>(0x5, ctx, vcpu, mtd);
}
EXPORT_PORTAL(mcr_mrc_2_handler, mword);

ZETA_PORTAL(ldc_stc_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: LDC/STC. ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(ldc_stc_handler, mword);

ZETA_PORTAL(sve_fpu_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: SVE/SIMD/FPU. ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(sve_fpu_handler, mword);

ZETA_PORTAL(vmrs_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::vmrs>(0x8, ctx, vcpu, mtd);
}
EXPORT_PORTAL(vmrs_handler, mword);

ZETA_PORTAL(pauth_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Pauth. ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(pauth_handler, mword);

ZETA_PORTAL(mrrc_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: MRRC. ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(mrrc_handler, mword);

ZETA_PORTAL(illegal_exec_state_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unrecoverable VM Exit: Illegal execution state. ESR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(illegal_exec_state_handler, mword);

ZETA_PORTAL(svc_32_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: SVC (AArch32). ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(svc_32_handler, mword);

ZETA_PORTAL(hvc_32_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: HVC (AArch32). ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(hvc_32_handler, mword);

ZETA_PORTAL(smc_32_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::smc>(0x13, ctx, vcpu, mtd);
}
EXPORT_PORTAL(smc_32_handler, mword);

ZETA_PORTAL(svc_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: SVC (AArch64). ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(svc_handler, mword);

ZETA_PORTAL(hvc_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: HVC (AArch64). ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(hvc_handler, mword);

ZETA_PORTAL(sve_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: SVE. ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(sve_handler, mword);

ZETA_PORTAL(eret_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: ERET. ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(eret_handler, mword);

ZETA_PORTAL(inst_abort_lower_el_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd,
            const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::instruction_abort>(Nova::Exc::IABORT_EL, ctx, vcpu, mtd);
}
EXPORT_PORTAL(inst_abort_lower_el_handler, mword);

ZETA_PORTAL(inst_abort_same_el_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH(
        "Unrecoverable VM Exit: Instruction abort (same EL). ESR_EL2: 0x%llx FAR_EL2: 0x%llx",
        ctx->utcb()->arch.el2_esr, ctx->utcb()->arch.el2_far);
    return 0;
}
EXPORT_PORTAL(inst_abort_same_el_handler, mword);

ZETA_PORTAL(pc_unaligned_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unrecoverable VM Exit: PC alignment fault. ESR_EL2: 0x%llx ELR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr, ctx->utcb()->arch.el2_elr);
    return 0;
}
EXPORT_PORTAL(pc_unaligned_handler, mword);

ZETA_PORTAL(data_abort_same_el_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unrecoverable VM Exit: Data abort (same EL). ESR_EL2: 0x%llx FAR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr, ctx->utcb()->arch.el2_far);
    return 0;
}
EXPORT_PORTAL(data_abort_same_el_handler, mword);

ZETA_PORTAL(sp_unaligned_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unrecoverable VM Exit: SP alignment fault. ESR_EL2: 0x%llx SP_EL1: 0x%llx",
               ctx->utcb()->arch.el2_esr, ctx->utcb()->arch.el1_sp);
    return 0;
}
EXPORT_PORTAL(sp_unaligned_handler, mword);

ZETA_PORTAL(trapped_fpu_32_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Trapped FPU (AArch32). ESR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(trapped_fpu_32_handler, mword);

ZETA_PORTAL(trapped_fpu_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Trapped FPU (AArch64). ESR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(trapped_fpu_handler, mword);

ZETA_PORTAL(serror_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Serror. ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(serror_handler, mword);

ZETA_PORTAL(brkpt_lower_el_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Breakpoint (lower EL). ESR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(brkpt_lower_el_handler, mword);

ZETA_PORTAL(brkpt_same_el_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Breakpoint (same EL). ESR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(brkpt_same_el_handler, mword);

ZETA_PORTAL(soft_step_lower_el_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd,
            const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::single_step>(0x32, ctx, vcpu, mtd);
}
EXPORT_PORTAL(soft_step_lower_el_handler, mword);

ZETA_PORTAL(soft_step_same_el_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Software step (same EL). ESR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(soft_step_same_el_handler, mword);

ZETA_PORTAL(watchpoint_lower_el_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Watchpoint (lower EL). ESR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(watchpoint_lower_el_handler, mword);

ZETA_PORTAL(watchpoint_same_el_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Watchpoint (same EL). ESR_EL2: 0x%llx",
               ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(watchpoint_same_el_handler, mword);

ZETA_PORTAL(bkpt_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd_in, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::bkpt>(0x38, ctx, vcpu, mtd_in);
}
EXPORT_PORTAL(bkpt_handler, mword);

ZETA_PORTAL(vector_catch_handler, Vcpu::Vcpu&, Nova::Mtd, const Zeta::Zeta_ctx* ctx) {
    ABORT_WITH("Unsupported VM Exit: Vector catch. ESR_EL2: 0x%llx", ctx->utcb()->arch.el2_esr);
    return 0;
}
EXPORT_PORTAL(vector_catch_handler, mword);

ZETA_PORTAL(brk_handler, Vcpu::Vcpu& vcpu, Nova::Mtd mtd_in, const Zeta::Zeta_ctx* ctx) {
    return call_portal_handler<Vmexit::brk>(Nova::Exc::BRK, ctx, vcpu, mtd_in);
}
EXPORT_PORTAL(brk_handler, mword);

#ifndef __clang__
#pragma GCC diagnostic pop
#endif

Portal_entry_config portals_config[] = {
    {0, _nova_pt_unknown_reason_handler, 0}, // 0x0 - Unknown reason
    {Nova::MTD::EL2_ELR_SPSR | Nova::MTD::TMR | Nova::MTD::GIC | Nova::MTD::EL2_ESR_FAR,
     _nova_pt_wfie_handler, 0},                              // 0x1 - WF(I|E)
    {0, nullptr, 0},                                         // 0x2 - Reserved
    {Portal::MTD_MSR_COMMON, _nova_pt_mcr_mrc_handler, 0},   // 0x3 - MCR/MRC
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_mcrr_mrrc_handler, 0}, // 0x4 - MCRR/MRRC
    {Portal::MTD_MSR_COMMON, _nova_pt_mcr_mrc_2_handler, 0}, // 0x5 - MCR/MRC 2
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_ldc_stc_handler, 0},   // 0x6 - LDC/STC
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_sve_fpu_handler, 0},   // 0x7 - SVE/FPU
    {Portal::MTD_MSR_COMMON, _nova_pt_vmrs_handler, 0},      // 0x8 - VMRS
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_pauth_handler, 0},     // 0x9 - Pauth
    {0, nullptr, 0},                                         // 0xa - Reserved
    {0, nullptr, 0},                                         // 0xb - Reserved
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_mrrc_handler, 0},      // 0xc - MRRC
    {0, nullptr, 0},                                         // 0xd - Reserved
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_illegal_exec_state_handler,
     0},                                                  // 0xe - Illegal Execution state
    {0, nullptr, 0},                                      // 0xf - Reserved
    {0, nullptr, 0},                                      // 0x10 - Reserved
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_svc_32_handler, 0}, // 0x11 - SVC 32
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_hvc_32_handler, 0}, // 0x12 - HVC 32
    {Nova::MTD::GPR | Nova::MTD::EL2_ELR_SPSR | Nova::MTD::TMR | Nova::MTD::GIC,
     _nova_pt_smc_32_handler, 0},                      // 0x13 - SMC 32
    {0, nullptr, 0},                                   // 0x14 - Reserved
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_svc_handler, 0}, // 0x15 - SVC 64
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_hvc_handler, 0}, // 0x16 - HVC 64
    {Nova::MTD::GPR | Nova::MTD::EL2_ELR_SPSR | Nova::MTD::TMR | Nova::MTD::GIC,
     _nova_pt_smc_handler, 0},                             // 0x17 - SMC 64
    {Portal::MTD_MSR_COMMON, _nova_pt_msr_mrs_handler, 0}, // 0x18 - MSR/MRS
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_sve_handler, 0},     // 0x19 - SVE
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_eret_handler, 0},    // 0x1a - ERET
    {0, nullptr, 0},                                       // 0x1b - reserved
    {0, nullptr, 0},                                       // 0x1c - reserved
    {0, nullptr, 0},                                       // 0x1d - reserved
    {0, nullptr, 0},                                       // 0x1e - reserved
    {0, nullptr, 0},                                       // 0x1f - reserved
    {Nova::MTD::EL2_ESR_FAR | Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL2_HPFAR
         | Nova::MTD::EL1_ESR_FAR | Nova::MTD::GIC,
     _nova_pt_inst_abort_lower_el_handler, 0}, // 0x20 - Instruction abort (lower EL)
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_inst_abort_same_el_handler,
     0}, // 0x21 - Instruction abort (same EL)
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_pc_unaligned_handler, 0}, // 0x22 - PC alignment fault
    {0, nullptr, 0},                                            // 0x23 - reserved
    {Nova::MTD::EL2_HPFAR | Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL2_ESR_FAR | Nova::MTD::GPR
         | Nova::MTD::GIC,
     _nova_pt_dabort_handler, 0}, // 0x24 - Data abort (lower EL)
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_data_abort_same_el_handler, 0}, // 0x25 - Data abort (same EL)
    {Nova::MTD::EL2_ESR_FAR | Nova::MTD::EL1_SP, _nova_pt_sp_unaligned_handler,
     0},                                                          // 0x26 - PC alignment fault
    {0, nullptr, 0},                                              // 0x27 - reserved
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_trapped_fpu_32_handler, 0}, // 0x28 - Trapped FPU 32
    {0, nullptr, 0},                                              // 0x29 - reserved
    {0, nullptr, 0},                                              // 0x2a - reserved
    {0, nullptr, 0},                                              // 0x2b - reserved
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_trapped_fpu_handler, 0},    // 0x2c - Trapped FPU 64
    {0, nullptr, 0},                                              // 0x2d - reserved
    {0, nullptr, 0},                                              // 0x2e - reserved
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_serror_handler, 0},         // 0x2f - Serror
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_brkpt_lower_el_handler, 0}, // 0x30 - Breakpoint (lower EL)
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_brkpt_same_el_handler, 0},  // 0x31 - Breakpoint (same EL)
    {Nova::MTD::EL2_ESR_FAR | Nova::MTD::EL2_ELR_SPSR | Nova::MTD::GIC,
     _nova_pt_soft_step_lower_el_handler, 0}, // 0x32 - Software Step (lower EL)
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_soft_step_same_el_handler,
     0}, // 0x33 - Software Step (same EL)
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_watchpoint_lower_el_handler,
     0}, // 0x34 - Watchpoint (lower EL)
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_watchpoint_same_el_handler, 0}, // 0x35 - Watchpoint (same EL)
    {0, nullptr, 0},                                                  // 0x36 - reserved
    {0, nullptr, 0},                                                  // 0x37 - reserved
    {Nova::MTD::EL2_ESR_FAR | Nova::MTD::EL1_VBAR | Nova::MTD::EL1_ESR_FAR | Nova::MTD::EL2_ELR_SPSR
         | Nova::MTD::GIC,
     _nova_pt_bkpt_handler, 0},                                 // 0x38 - BKPT
    {0, nullptr, 0},                                            // 0x39 - reserved
    {Nova::MTD::EL2_ESR_FAR, _nova_pt_vector_catch_handler, 0}, // 0x3a - Vector catch
    {0, nullptr, 0},                                            // 0x3b - reserved
    {Nova::MTD::EL2_ESR_FAR | Nova::MTD::EL1_VBAR | Nova::MTD::EL1_ESR_FAR | Nova::MTD::EL2_ELR_SPSR
         | Nova::MTD::GIC,
     _nova_pt_brk_handler, 0}, // 0x3c - BRK
    {0, nullptr, 0},           // 0x3d - reserved
    {0, nullptr, 0},           // 0x33 - reserved
    {0, nullptr, 0},           // 0x3f - reserved
    {Portal::MTD_CPU_STARTUP_INFO | Nova::MTD::EL2_IDR | Nova::MTD::TMR | Nova::MTD::EL2_ELR_SPSR,
     _nova_pt_startup_handler, 0},                                          // 0x40 - Startup (NOVA)
    {Nova::MTD::GIC | Nova::MTD::EL2_ELR_SPSR, _nova_pt_recall_handler, 0}, // 0x41 - Recall (NOVA)
    {Nova::MTD::GIC | Nova::MTD::TMR | Nova::MTD::EL2_ELR_SPSR, _nova_pt_vtimer_handler,
     0}, // 0x42 - VTimer (NOVA)
};

Errno
Portal::ctrl_portal(Sel base_sel, Sel id, Vcpu::Vcpu& vcpu) {
    ASSERT(id < Nova::Exc::VCPU_COUNT);

    return Zeta::ctrl_pt(base_sel + id, reinterpret_cast<mword>(&vcpu),
                         portals_config[id].mtd | portals_config[id].extra_regs);
}

void
Portal::add_regs(Sel id, Nova::Mtd mtd) {
    ASSERT(id < Nova::Exc::VCPU_COUNT);
    portals_config[id].extra_regs |= mtd;
}

void
Portal::remove_regs(Sel id, Nova::Mtd mtd) {
    ASSERT(id < Nova::Exc::VCPU_COUNT);
    portals_config[id].extra_regs &= ~mtd;
}

void
Portal::clear_regs(Sel id) {
    ASSERT(id < Nova::Exc::VCPU_COUNT);
    portals_config[id].extra_regs = 0;
}

Errno
Portal::init_portals(Zeta::Local_ec& lec, Sel exc_base_sel, Vcpu::Vcpu& vcpu) {
    for (size_t i = 0; i < sizeof(portals_config) / sizeof(portals_config[0]); ++i) {
        if (portals_config[i].entry == nullptr)
            continue;

        Errno err;
        err = lec.bind(exc_base_sel + i, portals_config[i].entry, reinterpret_cast<mword>(&vcpu),
                       portals_config[i].mtd);
        if (err != Errno::ENONE)
            return err;
    }

    return ENONE;
}