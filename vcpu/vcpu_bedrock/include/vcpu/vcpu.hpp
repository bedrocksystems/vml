/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <model/cpu.hpp>
#include <model/physical_timer.hpp>
#include <model/vcpu_types.hpp>
#include <msr/esr.hpp>
#include <msr/msr.hpp>
#include <platform/errno.hpp>
#include <platform/reg_accessor.hpp>
#include <platform/types.hpp>

namespace Vcpu {
    class Vcpu;
};

namespace Model {
    class Board;
};

/*! \brief Concrete implementation of a VCPU when running on BedRock
 */
class Vcpu::Vcpu : public Model::Cpu {
private:
    bool _aarch64{true};

    uint32 _elrsr_used{0};

    Sel _vcpu_sel{Sels::INVALID}, _lec_sel{Sels::INVALID}, _exc_base_sel{Sels::INVALID},
        _sc_sel{Sels::INVALID}, _sm_sel{Sels::INVALID};

    Nova::Mtd reset(Reg_accessor &utcb);

public:
    enum Exception_class : uint64 {
        SAME_EL_SP0 = 0x0,
        SAME_EL_SPX = 0x200,
        LOWER_EL_AA64 = 0x400,
        LOWER_EL_AA32 = 0x600
    };

    enum Exception_type : uint64 { SYNC = 0x0, IRQ = 0x80, FIQ = 0x100, SERR = 0x180 };

    static constexpr unsigned MAX_IRQ_RT
        = sizeof(Nova::Utcb_arch::gic_lr) / sizeof(Nova::Utcb_arch::gic_lr[0]);

    Model::Physical_timer ptimer;
    Model::Board *const board;
    Msr::Bus msr_bus;

    Vcpu(Model::Board &, Vcpu_id, Pcpu_id, uint16, uint16, bool, bool);

    Errno setup(const Zeta::Zeta_ctx *);

    Nova::Mtd reconfigure(const Platform_ctx &, const Nova::Mtd mtd_in);

    Vbus::Err handle_instruction_abort(const Vcpu_ctx *vcpu_ctx, uint64 const fault_paddr,
                                       Esr::Instruction_abort const &esr);
    Vbus::Err handle_data_abort(const Vcpu_ctx *vcpu_ctx, uint64 const fault_paddr,
                                Esr::Data_abort const &esr, uint64 &reg_value);
    Vbus::Err handle_msr_exit(const Vcpu_ctx *vcpu_ctx, Msr::Access const &msr_info,
                              uint64 &reg_value);
    Nova::Mtd update_inj_status(const Platform_ctx &, const Nova::Mtd mtd_in);
    Nova::Mtd inject_irqs(const Platform_ctx &, const Nova::Mtd mtd_in);

    Nova::Mtd forward_exception(const Platform_ctx &, const Nova::Mtd mtd_in, Exception_class c,
                                Exception_type t, bool update_far);

    virtual Errno run(const Zeta::Zeta_ctx *ctx) override;
    virtual void ctrl_tvm(bool enable,
                          Request::Requestor requestor = Request::Requestor::REQUESTOR_VMM,
                          const Nova::Mtd regs = 0) override;
    virtual bool block() override {
        Errno err = Zeta::sm_down(_sm_sel, 0, true);
        return err == ENONE;
    }

    virtual void block_timeout(uint64 const absolut_timeout) override {
        Zeta::sm_down(_sm_sel, absolut_timeout, true);
    }

    virtual bool unblock() override {
        Errno err = Zeta::sm_up(_sm_sel);
        return (err == ENONE);
    }

    virtual bool recall() override {
        Errno err = Zeta::recall(_vcpu_sel, false);
        return (err == ENONE);
    }

    bool aarch64() const { return _aarch64; }
};