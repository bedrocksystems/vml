/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <alloc/sels.hpp>
#include <alloc/vmap.hpp>
#include <arch/barrier.hpp>
#include <bedrock/portal.hpp>
#include <guest_config/guest.hpp>
#include <log/log.hpp>
#include <model/board.hpp>
#include <model/guest_as.hpp>
#include <model/vcpu_types.hpp>
#include <msr/msr.hpp>
#include <msr/msr_info.hpp>
#include <new.hpp>
#include <outpost/outpost.hpp>
#include <platform/reg_accessor.hpp>
#include <vbus/vbus.hpp>
#include <vcpu/request.hpp>
#include <vcpu/vcpu.hpp>
#include <vcpu/vcpu_roundup.hpp>
#include <zeta/zeta.hpp>

Vcpu::Vcpu::Vcpu(Model::Board& b, Vcpu_id vcpu_id, Pcpu_id pcpu, uint16 const vtimer_irq,
                 uint16 const ptimer_irq, bool ptimer_edge, bool is_64_bit)
    : Model::Cpu(*b.get_gic(), vcpu_id, pcpu, vtimer_irq), _aarch64(is_64_bit),
      ptimer(*b.get_gic(), vcpu_id, ptimer_irq, ptimer_edge), board(&b) {
}

Nova::Mtd
Vcpu::Vcpu::reset(Reg_accessor& arch) {

    arch.set_reg_selection_out(Nova::MTD::GIC | Nova::MTD::TMR | Nova::MTD::GPR | Nova::MTD::EL2_HCR
                               | Nova::MTD::EL2_ELR_SPSR | Nova::MTD::EL2_IDR | Nova::MTD::EL1_SCTLR
                               | Nova::MTD::EL1_ELR_SPSR | Portal::MTD_CPU_STARTUP_INFO);

    arch.reset_gpr();
    arch.el1_sp(0, true);
    arch.el1_tpidr(0, true);
    arch.el1_contextidr(0, true);
    arch.el1_elr(0, true);
    arch.el1_spsr(0, true);
    arch.el1_esr(0, true);
    arch.el1_far(0, true);
    arch.el1_afsr0(0, true);
    arch.el1_afsr1(0, true);
    arch.el1_ttbr0(0, true);
    arch.el1_ttbr1(0, true);
    arch.el1_tcr(0, true);
    arch.el1_mair(0, true);
    arch.el1_amair(0, true);
    arch.el1_vbar(0, true);
    arch.el1_sctlr(0, true);
    arch.reset_gic();

    _elrsr_used = 0;
    reset_interrupt_state();
    arch.el2_vmpidr(uint64(aff3()) << 32 | (1ULL << 31) | (uint64(aff2()) << 16)
                        | (uint64(aff1()) << 8) | uint64(aff0()),
                    true);

    msr_bus.reset();

    /* setup guest registers */
    uint64 el2_spsr, el2_hcr;
    arch.el2_elr(boot_addr(), true);
    arch.tmr_reset(timer_offset());
    el2_hcr = Msr::Info::HCR_EL2_DEFAULT_VALUE;
    arch.el1_sctlr(Msr::Info::SCTLR_EL1_DEFAULT_VALUE, true);

    if (aarch64()) {
        el2_spsr = Msr::Info::D_MASKED | Msr::Info::AIF_MASKED | Msr::Info::AARCH64
                   | Msr::Info::AA64_EL1 | Msr::Info::AA64_SPX;
        el2_hcr |= Msr::Info::HCR_EL2_RW;
        arch.gpr(0, boot_arg(), true);
    } else {

        el2_spsr = Msr::Info::AIF_MASKED | Msr::Info::AARCH32 | Msr::Info::AA32_SVC;
        arch.gpr(0, 0ull, true);
        arch.gpr(1, LINUX_MACHINE_TYPE_DTB, true);
        arch.gpr(2, boot_arg(), true);
    }
    arch.el2_hcr(el2_hcr, true);
    arch.el2_spsr(el2_spsr, true);
    arch.el1_spsr(el2_spsr, true);

    /*
     * Mark the features that are disabled on reset - if they are still requested, the
     * reconfigure code will detect that they need to be re-enabled.
     */
    _singe_step.set_enabled(false);
    _tvm.set_enabled(false);

    INFO("VCPU %u jumping to guest code @ 0x%llx in mode 0x%x", id(), boot_addr(),
         el2_spsr & Msr::Info::SPSR_MODE_MASK);
    return arch.get_reg_selection_out();
}

Nova::Mtd
Vcpu::Vcpu::check_reset(const Platform_ctx& ctx, Nova::Mtd mtd_in) {
    Nova::Mtd mtd_out = 0;
    Reg_accessor arch(ctx, mtd_in);

    if (_reset.is_requested()) {
        mtd_out = reset(arch);
        _reset.unset_requests();
    }

    return mtd_out;
}

/*! \brief VCPU internal reconfiguration
 *
 * This code is called has the last step of every VM exit. The VCPU is not considered
 * to be in emulation inside this function.
 */
Nova::Mtd
Vcpu::Vcpu::reconfigure(const Platform_ctx& ctx, const Nova::Mtd mtd_in) {
    Nova::Mtd mtd_out = 0;
    Reg_accessor arch(ctx, mtd_in);

    if (_tvm.needs_reconfiguration()) {
        uint64 el2_hcr = Msr::Info::HCR_EL2_DEFAULT_VALUE;

        arch.set_reg_selection_out(Nova::MTD::EL2_HCR);

        if (aarch64())
            el2_hcr |= Msr::Info::HCR_EL2_RW;
        if (!_tvm.is_enabled()) {
            el2_hcr |= Msr::Info::HCR_EL2_TVM;
        }
        arch.el2_hcr(el2_hcr, true);

        mtd_out |= arch.get_reg_selection_out();
        _tvm.set_enabled(!_tvm.is_enabled());
    }

    if (_singe_step.needs_reconfiguration()) {
        uint64 el1_mdscr = 0, el2_spsr = arch.el2_spsr();
        arch.set_reg_selection_out(Nova::MTD::EL1_MDSCR | Nova::MTD::EL2_ELR_SPSR);

        if (!_singe_step.is_enabled()) {
            el1_mdscr |= Msr::Info::MDSCR_SINGLE_STEP;
            el2_spsr |= Msr::Info::SPSR_SINGLE_STEP;
        } else {
            el2_spsr &= ~Msr::Info::SPSR_SINGLE_STEP;
        }

        arch.el1_mdscr(el1_mdscr, true);
        arch.el2_spsr(el2_spsr);
        mtd_out |= arch.get_reg_selection_out();
        _singe_step.set_enabled(!_singe_step.is_enabled());
    }

    return mtd_out;
}

/*! \brief Update the VCPU internal state when enabling or disabling Trap Virtual Memory (TVM)
 *
 *  This function will coordinate the global state of TVM when multiple clients are disabling
 *  or enabling this feature. If at least one client wants TVM enabled, it will be enabled.
 *  Otherwise, it is de-activated. Callers can also request extra registers to be populated when
 *  taking a VM exit due to a TVM access. Consequently, there are two cases:
 *  1 - We need to disable/enable the feature globally. We then clear the extra regs or add the
 *      basic set of regs needed by the VMM on first enablement.
 *  2 - The caller wants extra registers (and potentially TVM was already enabled). We proceed
 *      with the addition of those registers and then reconfigure the portals.
 *
 *  Generally, when registers are changed, the portal needs to be re-configured. Note that for
 * now, we only have one caller outside of the VMM (VMI). So, we keep this simple. If we needed
 * to handle more callers, we would need to store the extra_regs for each caller and OR them
 * together to get the final value.
 */
void
Vcpu::Vcpu::ctrl_tvm(bool enable, Request::Requestor requestor, const Nova::Mtd regs) {
    bool needs_update = _tvm.needs_update(enable, requestor);

    if (enable) {
        Portal::add_regs(Nova::Exc::MSR_MRS, regs | Portal::MTD_MSR_TRAP_VM);
        Portal::add_regs(0x3, regs | Portal::MTD_MSR_TRAP_VM);
        Portal::add_regs(0x5, regs | Portal::MTD_MSR_TRAP_VM);
    } else {
        if (needs_update) { // We completely disable TVM
            Portal::clear_regs(Nova::Exc::MSR_MRS);
            Portal::clear_regs(0x3);
            Portal::clear_regs(0x5);
        } else { // Make sure that we don't remove the base set of regs
            Portal::remove_regs(Nova::Exc::MSR_MRS, regs & ~Portal::MTD_MSR_TRAP_VM);
            Portal::remove_regs(0x3, regs & ~Portal::MTD_MSR_TRAP_VM);
            Portal::remove_regs(0x5, regs & ~Portal::MTD_MSR_TRAP_VM);
        }
    }

    Errno err;
    err = Portal::ctrl_portal(_exc_base_sel, Nova::Exc::MSR_MRS, *this);
    if (__UNLIKELY__(err != ENONE)) {
        ABORT_WITH("Unable to reconfigure TVM for VCPU %lu", this->id());
    }
    err = Portal::ctrl_portal(_exc_base_sel, 0x3, *this); // MCR/MRC
    if (__UNLIKELY__(err != ENONE)) {
        ABORT_WITH("Unable to reconfigure TVM for VCPU %lu", this->id());
    }
    err = Portal::ctrl_portal(_exc_base_sel, 0x5, *this); // MCR/MRC 2nd portal
    if (__UNLIKELY__(err != ENONE)) {
        ABORT_WITH("Unable to reconfigure TVM for VCPU %lu", this->id());
    }
}

void
Vcpu::Vcpu::ctrl_single_step(bool enable, Request::Requestor requestor) {
    _singe_step.needs_update(enable, requestor);
}

Errno
Vcpu::Vcpu::setup(const Zeta::Zeta_ctx* ctx) {
    Errno err;
    ::Cpu const pcpu = _pcpu_id;

    bool ok = Model::Cpu::setup(ctx);
    if (!ok)
        return ENOMEM;

    _exc_base_sel = Sels::alloc(Nova::Exc::EC_COUNT);
    _sm_sel = Sels::alloc();

    if (_sm_sel == Sels::INVALID || _exc_base_sel == Sels::INVALID) {
        WARN("Unable to allocate selectors for vCPU %u", id());
        return ENOMEM;
    }

    INFO("Setting up vCPU %u -> %u pCPU", id(), pcpu);

    err = _lec.create(ctx->cpu());
    if (err != Errno::ENONE)
        return err;

    err = Portal::init_portals(_lec, _exc_base_sel, *this);
    if (err != Errno::ENONE)
        return err;

    err = _vcpu_ec.create(ctx->cpu(), _exc_base_sel);
    if (err != Errno::ENONE)
        return err;

    err = Zeta::create_sm(ctx, _sm_sel, 0);
    if (err != Errno::ENONE)
        return err;

    ok = ptimer.init(ctx);
    if (!ok)
        return Errno::EINVAL;

    err = timer_gec.start(
        ctx->cpu(), Nova::Qpd(),
        reinterpret_cast<Zeta::global_ec_entry>(Model::Physical_timer::timer_loop), &ptimer);
    if (err != Errno::ENONE)
        return err;

    ptimer.wait_for_loop_start();

    DEBUG("VCPU %u is setup", id());

    return Errno::ENONE;
}

Errno
Vcpu::Vcpu::run() {
    switch_state_to_on();
    return _vcpu_ec.run(Nova::Qpd());
}

Nova::Mtd
Vcpu::Vcpu::update_inj_status(const Platform_ctx& ctx, const Nova::Mtd mtd_in) {
    if (!_elrsr_used)
        return 0;

    /* either IRQ is complete or not yet injected - both cases are important */
    Reg_accessor arch(ctx, mtd_in);
    uint32 const check = arch.gic_elrsr() & _elrsr_used;

    arch.set_reg_selection_out(Nova::MTD::GIC);

    for (uint8 i = 0; i < MAX_IRQ_RT; i++) {
        if (!(check & (1u << i)))
            continue;

        /*
         * If not injected yet, put back nevertheless to the GIC and
         * re-request later on inject_irqs(). This is required for 2 reasons.
         * First, GIC may have more high priority IRQs pendings in the
         * meanwhile, which matters especially when gic_lr[] is fully
         * loaded with IRQs to be injected.
         * Second, depending on the used hardware, solely some leading
         * gic_lr[] positions are actually recognized (e.g. 4 out of 16),
         * which requires to move to be injected IRQs (lr) to the top
         * of the array by putting them back to GIC and later re-request
         * on inject_irq.
         */
        _gic->update_inj_status(id(), arch.gic_lr(i));

        _elrsr_used &= ~(1u << i);
        arch.gic_lr(i, 0); /* invalidate lr */
    }

    return arch.get_reg_selection_out();
}

Nova::Mtd
Vcpu::Vcpu::forward_exception(const Platform_ctx& ctx, const Nova::Mtd mtd_in, Exception_class c,
                              Exception_type t, bool update_far) {
    Reg_accessor arch(ctx, mtd_in);
    Nova::Mtd mtd_out = Nova::MTD::EL1_ELR_SPSR | Nova::MTD::EL1_ESR_FAR | Nova::MTD::EL2_ELR_SPSR;

    arch.set_reg_selection_out(mtd_out);

    arch.el1_elr(arch.el2_elr(), true);
    arch.el2_elr(arch.el1_vbar() + c + t);
    arch.el1_spsr(arch.el2_spsr(), true);
    arch.el1_esr(arch.el2_esr());

    if (update_far)
        arch.el1_far(arch.el2_far());

    return arch.get_reg_selection_out();
}

Nova::Mtd
Vcpu::Vcpu::inject_irqs(const Platform_ctx& ctx, const Nova::Mtd mtd_in) {
    Reg_accessor arch(ctx, mtd_in);
    const uint32 gic_elrsr = arch.gic_elrsr();

    arch.set_reg_selection_out(Nova::MTD::GIC);

    for (uint8 i = 0; i < MAX_IRQ_RT; i++) {
        if (!(gic_elrsr & (1u << i)))
            continue;

        uint64 lr = 0;
        if (!pending_irq(lr))
            break;

        // Rare case where the IRQ just changed state beneath us, let's try again.
        if (__UNLIKELY__(lr == 0)) {
            continue;
        }

        _elrsr_used |= (1u << i);
        arch.gic_lr(i, lr);
    }

    return arch.get_reg_selection_out();
}

Vbus::Err
Vcpu::Vcpu::handle_data_abort(const Vcpu_ctx* vcpu_ctx, uint64 const fault_paddr,
                              Esr::Data_abort const& esr, uint64& reg_value) {
    Vbus::Access const access = esr.write() ? Vbus::Access::WRITE : Vbus::Access::READ;
    uint8 const bytes = esr.isv() ? esr.access_size_bytes() : Vbus::SIZE_UNKNOWN;
    Vbus::Err err = board->get_bus()->access(access, vcpu_ctx, fault_paddr, bytes, reg_value);

    if (err == Vbus::Err::OK && !esr.write()) {
        return Vbus::Err::UPDATE_REGISTER; // This is a read, request a GPR update
    }

    return err;
}

Vbus::Err
Vcpu::Vcpu::handle_instruction_abort(const Vcpu_ctx* vcpu_ctx, uint64 const fault_paddr,
                                     Esr::Instruction_abort const& esr) {
    uint64 dummy;
    const uint8 bytes = esr.instruction_len_bytes();

    return board->get_bus()->access(Vbus::Access::EXEC, vcpu_ctx, fault_paddr, bytes, dummy);
}

Vbus::Err
Vcpu::Vcpu::handle_msr_exit(const Vcpu_ctx* vcpu_ctx, Msr::Access const& msr_info,
                            uint64& reg_value) {
    Vbus::Access const access = msr_info.write() ? Vbus::Access::WRITE : Vbus::Access::READ;
    Vbus::Err err = msr_bus.access(access, vcpu_ctx, msr_info.id(), sizeof(uint64), reg_value);

    if (err == Vbus::Err::OK && !msr_info.write()) {
        // This is a read, request a GPR update
        return Vbus::Err::UPDATE_REGISTER;
    }

    return err;
}

void
Vcpu::Vcpu::advance_pc(const Vcpu_ctx& ctx, Reg_accessor& arch) {
    arch.advance_pc();

    if (_singe_step.is_requested_by(Request::Requestor::VMI))
        outpost::vmi_handle_singlestep(ctx);
}
