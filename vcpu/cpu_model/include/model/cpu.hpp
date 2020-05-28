/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/context.hpp>
#include <platform/errno.hpp>
#include <platform/semaphore.hpp>
#include <platform/types.hpp>
#include <platform/vm_types.hpp>
#include <vcpu/request.hpp>

namespace Model {
    class Cpu_irq_interface;
    class Cpu;
    class Cpu_feature;
    class Timer;
    class Gic_d;
    class Gic_r;
}

namespace Vbus {
    class Bus;
}

class Model::Cpu_irq_interface {
public:
    virtual void interrupt_pending() = 0;
    virtual Model::Gic_r *gic_r() = 0;

    virtual uint8 aff0() const = 0;
    virtual uint8 aff1() const = 0;
    virtual uint8 aff2() const = 0;
    virtual uint8 aff3() const = 0;
};

class Model::Cpu_feature {
public:
    bool is_requested_by(Request::Requestor requestor) const {
        return Request::is_requested_by(requestor, _requests);
    }
    bool is_requested() const { return static_cast<bool>(_requests.load()); }
    bool is_enabled() const { return _enabled; }
    void set_enabled(bool e);
    bool needs_reconfiguration() const { return is_enabled() != is_requested(); }
    bool needs_update(bool enable, Request::Requestor requestor) {
        return Request::needs_update(requestor, enable, _requests);
    }
    void unset_requests() { _requests = 0; }

private:
    atomic<uint32> _requests{0};
    bool _enabled;
};

class Model::Cpu : private Model::Cpu_irq_interface {
private:
    Semaphore _resume_sm;
    Semaphore _off_sm;
    uint64 _boot_addr{0};
    uint64 _boot_arg{0};

    Vcpu_id const _vcpu_id;
    uint16 _timer_irq;
    Model::Timer *_timer{nullptr};

    enum Interrupt_state { NONE, SLEEPING, PENDING };
    atomic<Interrupt_state> _interrupt_state{NONE};

    /*! \brief State Machine for the state of the VCPU
     *
     * Essentially, a VCPU can be in three states: ON, OFF or EMULATE.
     * OFF means that the VCPU is not in use by the guest OS (no yet started
     * or stopped). ON means that the VCPU is running normally (it can be in
     * the guest or in the VMM). When a VCPU is ON, it is _not_ emulating guest
     * progress. EMULATE means that guest execution is making progress in the VMM
     * via emulation. ROUNDEDUP is used to signal that a caller is asking for all
     * VCPUs to stop making progress. In that case, a VCPU an only transition from
     * XYZ to XYZ_ROUNDEDUP. Similarly, when a roundup is finished, it can only go
     * from XYZ_ROUNDEDUP to XYZ. The important point to note here is that a VCPU
     * cannot start emulating if it has been rounded up. In other words, the VCPU
     * cannot transition from ON_ROUNDED to EMULATE. Only ON to EMULATE is allowed.
     * EMULATE_ROUNDEDUP means that the CPU is emulating and a roundup is waiting for
     * it to complete.
     */
    enum State {
        OFF = 0,
        OFF_ROUNDEDUP = 1,
        ON = 2,
        ON_ROUNDEDUP = 3,
        EMULATE = 4,
        EMULATE_ROUNDEDUP = 5,
    };

    atomic<State> _state{OFF};

    bool is_turned_on_by_guest() const {
        return !_execution_paused.is_requested_by(Request::Requestor::VMM);
    }

    void resume_vcpu() { _resume_sm.release(); }
    void switch_on() { _off_sm.release(); }
    void resume();
    void set_reset_parameters(uint64 const boot_addr, uint64 const boot_arg, uint64 const tmr_off);

    virtual void ctrl_tvm(bool enable, Request::Requestor requestor = Request::Requestor::VMM,
                          const Reg_selection regs = 0)
        = 0;
    virtual void ctrl_single_step(bool enable,
                                  Request::Requestor requestor = Request::Requestor::VMM)
        = 0;

    static void roundup(Vcpu_id);

protected:
    uint64 _tmr_off{0};

    Pcpu_id const _pcpu_id;
    Model::Gic_d *const _gic;
    Model::Gic_r *_gic_r{nullptr};

    void wait_for_switch_on() { _off_sm.acquire(); }
    uint64 boot_addr() const { return _boot_addr; }
    uint64 boot_arg() const { return _boot_arg; }
    void switch_state_to_off();
    uint64 timer_offset() const { return _tmr_off; }
    void reset_interrupt_state() { _interrupt_state = NONE; }

    Cpu_feature _reset;
    Cpu_feature _tvm;
    Cpu_feature _singe_step;
    Cpu_feature _execution_paused;

public:
    // VCPU api start

    static bool init(uint16 vcpus);
    static bool is_cpu_turned_on_by_guest(Vcpu_id);
    static void roundup_all();
    static void resume_all();
    static Errno run(Vcpu_id);

    typedef void (*ctrl_feature_cb)(Model::Cpu *, bool, Request::Requestor, Reg_selection);
    typedef bool (*requested_feature_cb)(Model::Cpu *, Request::Requestor);

    static void ctrl_feature_on_vcpu(ctrl_feature_cb cb, Vcpu_id, bool,
                                     Request::Requestor requestor = Request::Requestor::VMM,
                                     Reg_selection regs = 0);
    static void ctrl_feature_on_all_but_vcpu(ctrl_feature_cb cb, Vcpu_id, bool,
                                             Request::Requestor requestor = Request::Requestor::VMM,
                                             Reg_selection regs = 0);
    static void ctrl_feature_on_all_vcpus(ctrl_feature_cb cb, bool,
                                          Request::Requestor requestor = Request::Requestor::VMM,
                                          Reg_selection regs = 0);
    static bool is_feature_enabled_on_vcpu(requested_feature_cb cb, Vcpu_id,
                                           Request::Requestor requestor = Request::Requestor::VMM);

    static void ctrl_feature_off(Model::Cpu *vcpu, bool enable, Request::Requestor requestor,
                                 Reg_selection regs);
    static void ctrl_feature_reset(Model::Cpu *vcpu, bool enable, Request::Requestor requestor,
                                   Reg_selection regs);
    static void ctrl_feature_tvm(Model::Cpu *vcpu, bool enable, Request::Requestor requestor,
                                 Reg_selection regs);
    static void ctrl_feature_single_step(Model::Cpu *vcpu, bool enable,
                                         Request::Requestor requestor, Reg_selection regs);
    static bool requested_feature_tvm(Model::Cpu *vcpu, Request::Requestor requestor);

    static uint16 get_num_vcpus();
    static Pcpu_id get_pcpu(Vcpu_id);

    /*
     * Values are chosen to match the PSCI spec for conveniency. This could be changed
     * in the future if something else is needed.
     */
    enum Start_err { SUCCESS = 0, INVALID_PARAMETERS = -2, ALREADY_ON = -4, INVALID_ADDRESS = -9 };

    static Start_err start_cpu(Vcpu_id vcpu_id, Vbus::Bus &vbus, uint64 boot_addr, uint64 boot_arg,
                               uint64 timer_off);

    // VCPU api end

    // Functions that should be provided by the implementation
    virtual bool block() = 0;
    virtual void block_timeout(uint64) = 0;
    virtual bool unblock() = 0;
    virtual bool recall() = 0;
    virtual Errno run() = 0;

    // Functions that are implemented
    Cpu(Gic_d &gic, Vcpu_id vcpu_id, Pcpu_id pcpu_id, uint16 const irq);
    bool setup(const Platform_ctx *ctx);

    void switch_state_to_roundedup();
    void switch_state_to_on();
    bool switch_state_to_emulating();

    Vcpu_id id() const { return _vcpu_id; }

    virtual uint8 aff0() const override;
    virtual uint8 aff1() const override;
    virtual uint8 aff2() const override;
    virtual uint8 aff3() const override;

    void wait_for_resume() { _resume_sm.acquire(); }
    void assert_vtimer(uint64 const control);
    void wait_for_interrupt(uint64 const control, uint64 const timeout_absolut);
    void interrupt_pending() override;
    bool pending_irq(uint64 &lr);

    virtual Model::Gic_r *gic_r() override { return _gic_r; }
};