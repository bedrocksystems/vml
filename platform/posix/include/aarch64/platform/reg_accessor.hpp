/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/context.hpp>
#include <platform/vm_types.hpp>

// Registers that are directly accessible by the guest (bare-metal)
struct GuestRegs {
    uint64 x[31];
    uint64 el0_sp;
    uint64 el0_tpidr;
    uint64 el0_tpidrro;

    uint64 el1_sp;
    uint64 el1_tpidr;
    uint64 el1_contextidr;
    uint64 el1_elr;
    uint64 el1_spsr;
    uint64 el1_esr;
    uint64 el1_far;
    uint64 el1_afsr0;
    uint64 el1_afsr1;
    uint64 el1_ttbr0;
    uint64 el1_ttbr1;
    uint64 el1_tcr;
    uint64 el1_mair;
    uint64 el1_amair;
    uint64 el1_vbar;
    uint64 el1_sctlr;
    uint64 el1_mdscr;

    uint32 a32_spsr_abt;
    uint32 a32_spsr_fiq;
    uint32 a32_spsr_irq;
    uint32 a32_spsr_und;
    uint32 a32_dacr;
    uint32 a32_ifsr;

    uint64 el2_far;
    uint64 el2_esr;
    uint64 el2_elr;
    uint64 el2_spsr;
};

class RegAccessor {
public:
    RegAccessor() : _mtd_in(static_cast<uint64>(-1)), _mtd_out(static_cast<uint64>(-1)) {}
    RegAccessor(const Platform_ctx&, const Reg_selection) {}

    virtual inline Reg_selection get_reg_selection_in() const { return _mtd_in; }
    virtual inline Reg_selection get_reg_selection_out() const { return _mtd_out; }

    virtual inline uint64 gpr(uint8) const { return 0; }
    virtual inline void gpr(uint8, const uint64, bool overwrite = false) { (void)overwrite; }

    virtual inline uint64 tmr_cntvoff() const { return 0; }
    virtual inline uint64 el1_sctlr() const { return 0; }
    virtual inline uint64 el2_spsr() const { return 0; }

    virtual inline uint64 el2_elr() const { return 0; }

    virtual inline void el2_elr(const uint64) {}

    virtual inline void el2_spsr(const uint64) {}

    virtual inline uint64 pc() const { return el2_elr(); }
    virtual void set_from_guest_regs(const GuestRegs*, Reg_selection, Reg_selection) {}
    virtual void set_from_guest_regs(const GuestRegs*, Reg_selection) {}

    virtual void update_from_guest_regs(const GuestRegs*, Reg_selection) {}

    virtual const GuestRegs* get_guest_regs() { return &_guest; }

    virtual inline void advance_pc(uint8) {}
    virtual inline void advance_pc() {}

    virtual inline bool pc_advanced() const { return true; }

    virtual void advance_pc_once(void) {}

private:
    GuestRegs _guest;
    // Only set by constructor and [set_from_guest_regs]
    Reg_selection _mtd_in;
    Reg_selection _mtd_out{0};
};
