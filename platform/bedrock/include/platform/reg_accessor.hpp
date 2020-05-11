/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <nova/utcb.hpp>
#include <platform/context.hpp>
#include <platform/log.hpp>
#include <platform/string.hpp>
#include <platform/vm_types.hpp>
#include <zeta/types.hpp>

class Reg_accessor {
public:
    Reg_accessor(const Platform_ctx& ctx, const Reg_selection mtd_in)
        : _arch(&ctx.utcb()->arch), _mtd_in(mtd_in) {}

    static constexpr uint8 ZERO_REG_ID = 31;

    inline void set_reg_selection_out(Reg_selection mtd_out) { _mtd_out = mtd_out; }
    inline Nova::Mtd get_reg_selection_out() const { return _mtd_out; }

    inline uint64 el2_elr() const {
        ASSERT(_mtd_in & Nova::MTD::EL2_ELR_SPSR);
        return _arch->el2_elr;
    }

    inline void el2_elr(const uint64 val, bool overwrite = false) {
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL2_ELR_SPSR);
        ASSERT(_mtd_out & Nova::MTD::EL2_ELR_SPSR);
        _arch->el2_elr = val;
    }

    inline uint64 el2_spsr() const {
        ASSERT(_mtd_in & Nova::MTD::EL2_ELR_SPSR);
        return _arch->el2_spsr;
    }

    inline void el2_hcr(const uint64 val, bool overwrite = false) {
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL2_HCR);
        ASSERT(_mtd_out & Nova::MTD::EL2_HCR);
        _arch->el2_hcr = val;
    }

    inline uint64 el2_hcr() const {
        ASSERT(_mtd_in & Nova::MTD::EL2_HCR);
        return _arch->el2_hcr;
    }

    inline void el2_vpidr(const uint64 val) {
        ASSERT(_mtd_in & Nova::MTD::EL2_IDR);
        ASSERT(_mtd_out & Nova::MTD::EL2_IDR);
        _arch->el2_vpidr = val;
    }

    inline uint64 el2_vpidr() const {
        ASSERT(_mtd_in & Nova::MTD::EL2_IDR);
        return _arch->el2_vpidr;
    }

    inline void el2_vmpidr(const uint64 val, bool overwrite = false) {
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL2_IDR);
        ASSERT(_mtd_out & Nova::MTD::EL2_IDR);
        _arch->el2_vmpidr = val;
    }

    inline uint64 el2_vmpidr() const {
        ASSERT(_mtd_in & Nova::MTD::EL2_IDR);
        return _arch->el2_vmpidr;
    }

    inline void el2_spsr(const uint64 val, bool overwrite = false) {
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL2_ELR_SPSR);
        ASSERT(_mtd_out & Nova::MTD::EL2_ELR_SPSR);
        _arch->el2_spsr = val;
    }

    inline uint64 el2_esr() const {
        ASSERT(_mtd_in & Nova::MTD::EL2_ESR_FAR);
        return _arch->el2_esr;
    }

    inline uint64 el2_far() const {
        ASSERT(_mtd_in & Nova::MTD::EL2_ESR_FAR);
        return _arch->el2_far;
    }

    inline uint64 el2_hpfar() const {
        ASSERT(_mtd_in & Nova::MTD::EL2_HPFAR);
        return _arch->el2_hpfar;
    }

    inline void advance_pc() { el2_elr(el2_elr() + 4); }

    inline uint64 gpr(uint8 id) const {
        ASSERT(_mtd_in & Nova::MTD::GPR);
        ASSERT(id <= ZERO_REG_ID);
        return id == ZERO_REG_ID ? 0ull : _arch->x[id];
    }

    inline void gpr(uint8 id, const uint64 val, bool overwrite = false) {
        ASSERT(id <= ZERO_REG_ID);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::GPR);
        ASSERT(_mtd_out & Nova::MTD::GPR);
        if (id != ZERO_REG_ID) {
            _arch->x[id] = val;
        }
    }

    inline void reset_gpr() {
        ASSERT(_mtd_out & Nova::MTD::GPR);
        memset(_arch->x, 0, sizeof(_arch->x));
    }

    inline uint64 gic_lr(uint8 id) const {
        ASSERT(_mtd_in & Nova::MTD::GIC);
        ASSERT(id < (sizeof(_arch->gic_lr) / sizeof(_arch->gic_lr[0])));
        return _arch->gic_lr[id];
    }

    inline void gic_lr(uint8 id, const uint64 val) {
        ASSERT(id < (sizeof(_arch->gic_lr) / sizeof(_arch->gic_lr[0])));
        ASSERT(_mtd_out & Nova::MTD::GIC);
        ASSERT(_mtd_in & Nova::MTD::GIC);
        _arch->gic_lr[id] = val;
    }

    inline void reset_gic() {
        ASSERT(_mtd_out & Nova::MTD::GIC);
        memset(_arch->gic_lr, 0, sizeof(_arch->gic_lr));
    }

    inline uint32 gic_elrsr() const {
        ASSERT(_mtd_in & Nova::MTD::GIC);
        return _arch->gic_elrsr;
    }

    inline void a32_dacr(const uint32 val) {
        ASSERT(_mtd_out & Nova::MTD::A32_DACR_IFSR);
        ASSERT(_mtd_in & Nova::MTD::A32_DACR_IFSR);
        _arch->a32_dacr = val;
    }

    inline uint64 a32_dacr() const {
        ASSERT(_mtd_in & Nova::MTD::A32_DACR_IFSR);
        return _arch->a32_dacr;
    }

    inline void a32_ifsr(const uint32 val) {
        ASSERT(_mtd_out & Nova::MTD::A32_DACR_IFSR);
        ASSERT(_mtd_in & Nova::MTD::A32_DACR_IFSR);
        _arch->a32_ifsr = val;
    }

    inline uint64 a32_ifsr() const {
        ASSERT(_mtd_in & Nova::MTD::A32_DACR_IFSR);
        return _arch->a32_ifsr;
    }

    inline void el1_sp(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_SP);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_SP);
        _arch->el1_sp = val;
    }

    inline uint64 el1_sp() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_SP);
        return _arch->el1_sp;
    }

    inline void el1_tpidr(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_IDR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_IDR);
        _arch->el1_tpidr = val;
    }

    inline uint64 el1_tpidr() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_IDR);
        return _arch->el1_tpidr;
    }

    inline void el1_contextidr(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_IDR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_IDR);
        _arch->el1_contextidr = val;
    }

    inline uint64 el1_contextidr() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_IDR);
        return _arch->el1_contextidr;
    }

    inline void el1_elr(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_ELR_SPSR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_ELR_SPSR);
        _arch->el1_elr = val;
    }

    inline uint64 el1_elr() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_ELR_SPSR);
        return _arch->el1_elr;
    }

    inline void el1_spsr(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_ELR_SPSR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_ELR_SPSR);
        _arch->el1_spsr = val;
    }

    inline uint64 el1_spsr() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_ELR_SPSR);
        return _arch->el1_spsr;
    }

    inline void el1_esr(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_ESR_FAR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_ESR_FAR);
        _arch->el1_esr = val;
    }

    inline uint64 el1_esr() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_ESR_FAR);
        return _arch->el1_esr;
    }

    inline void el1_far(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_ESR_FAR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_ESR_FAR);
        _arch->el1_far = val;
    }

    inline uint64 el1_far() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_ESR_FAR);
        return _arch->el1_far;
    }

    inline void el1_afsr0(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_AFSR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_AFSR);
        _arch->el1_afsr0 = val;
    }

    inline uint64 el1_afsr0() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_AFSR);
        return _arch->el1_afsr0;
    }

    inline void el1_afsr1(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_AFSR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_AFSR);
        _arch->el1_afsr1 = val;
    }

    inline uint64 el1_afsr1() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_AFSR);
        return _arch->el1_afsr1;
    }

    inline void el1_ttbr0(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_TTBR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_TTBR);
        _arch->el1_ttbr0 = val;
    }

    inline uint64 el1_ttbr0() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_TTBR);
        return _arch->el1_ttbr0;
    }

    inline void el1_ttbr1(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_TTBR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_TTBR);
        _arch->el1_ttbr1 = val;
    }

    inline uint64 el1_ttbr1() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_TTBR);
        return _arch->el1_ttbr1;
    }

    inline void el1_tcr(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_TCR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_TCR);
        _arch->el1_tcr = val;
    }

    inline uint64 el1_tcr() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_TCR);
        return _arch->el1_tcr;
    }

    inline void el1_mair(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_MAIR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_MAIR);
        _arch->el1_mair = val;
    }

    inline uint64 el1_mair() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_MAIR);
        return _arch->el1_mair;
    }

    inline void el1_amair(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_MAIR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_MAIR);
        _arch->el1_amair = val;
    }

    inline uint64 el1_amair() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_MAIR);
        return _arch->el1_amair;
    }

    inline void el1_vbar(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_VBAR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_VBAR);
        _arch->el1_vbar = val;
    }

    inline uint64 el1_vbar() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_VBAR);
        return _arch->el1_vbar;
    }

    inline void el1_sctlr(const uint64 val, bool overwrite = false) {
        ASSERT(_mtd_out & Nova::MTD::EL1_SCTLR);
        if (!overwrite)
            ASSERT(_mtd_in & Nova::MTD::EL1_SCTLR);
        _arch->el1_sctlr = val;
    }

    inline uint64 el1_sctlr() const {
        ASSERT(_mtd_in & Nova::MTD::EL1_SCTLR);
        return _arch->el1_sctlr;
    }

    inline uint64 tmr_cntv_cval() const {
        ASSERT(_mtd_in & Nova::MTD::TMR);
        return _arch->tmr_cntv_cval;
    }

    inline void tmr_cntv_cval(const uint64 val) {
        ASSERT(_mtd_out & Nova::MTD::TMR);
        ASSERT(_mtd_in & Nova::MTD::TMR);
        _arch->tmr_cntv_cval = val;
    }

    inline uint64 tmr_cntv_ctl() const {
        ASSERT(_mtd_in & Nova::MTD::TMR);
        return _arch->tmr_cntv_ctl;
    }

    inline void tmr_cntv_ctl(const uint64 val) {
        ASSERT(_mtd_out & Nova::MTD::TMR);
        ASSERT(_mtd_in & Nova::MTD::TMR);
        _arch->tmr_cntv_ctl = val;
    }

    inline uint64 tmr_cntkctl() const {
        ASSERT(_mtd_in & Nova::MTD::TMR);
        return _arch->tmr_cntkctl;
    }

    inline void tmr_cntkctl(const uint64 val) {
        ASSERT(_mtd_out & Nova::MTD::TMR);
        ASSERT(_mtd_in & Nova::MTD::TMR);
        _arch->tmr_cntkctl = val;
    }

    inline uint64 tmr_cntvoff() const {
        ASSERT(_mtd_in & Nova::MTD::TMR);
        return _arch->tmr_cntvoff;
    }

    inline void tmr_cntvoff(const uint64 val) {
        ASSERT(_mtd_out & Nova::MTD::TMR);
        ASSERT(_mtd_in & Nova::MTD::TMR);
        _arch->tmr_cntvoff = val;
    }

    inline void tmr_reset(uint64 off = 0) {
        ASSERT(_mtd_out & Nova::MTD::TMR);
        _arch->tmr_cntvoff = off;
        _arch->tmr_cntkctl = 0;
        _arch->tmr_cntv_ctl = 0;
        _arch->tmr_cntv_cval = 0;
    }

private:
    Nova::Utcb_arch* _arch;
    const Reg_selection _mtd_in;
    Reg_selection _mtd_out{0};
};