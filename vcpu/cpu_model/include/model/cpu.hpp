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

class Model::Cpu : private Model::Cpu_irq_interface {
public:
    enum Vcpu_reconfiguration : uint64 {
        VCPU_RECONFIG_NONE = 0ull,
        VCPU_RECONFIG_TVM = 1ull << 1,
        VCPU_RECONFIG_RESET = 1ull << 2,
        VCPU_RECONFIG_SWITCH_OFF = 1ull << 3,
        VCPU_RECONFIG_SINGLE_STEP = 1ull << 4,
    };

private:
    Semaphore _resume_sm;
    Semaphore _off_sm;
    uint64 _boot_addr{0};
    uint64 _boot_arg{0};

    Vcpu_id const _vcpu_id;
    uint16 _timer_irq;
    Model::Timer *_timer{nullptr};

    atomic<uint64> _reconfig{VCPU_RECONFIG_NONE};

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

    bool is_on() {
        State s = _state;
        return s != OFF && s != OFF_ROUNDEDUP;
    }
    void resume_vcpu() { _resume_sm.release(); }
    void switch_on() { _off_sm.release(); }
    void resume();
    void set_reset_parameters(uint64 const boot_addr, uint64 const boot_arg, uint64 const tmr_off);

protected:
    uint64 _tmr_off{0};

    Pcpu_id const _pcpu_id;
    Model::Gic_d *const _gic;
    Model::Gic_r *_gic_r{nullptr};

    atomic<bool> _ss_enabled{false};
    atomic<uint32> _ss_requests{0};
    atomic<bool> _tvm_enabled{false};
    atomic<uint32> _tvm_requests{0};

    void wait_for_switch_on() { _off_sm.acquire(); }
    uint64 boot_addr() const { return _boot_addr; }
    uint64 boot_arg() const { return _boot_arg; }
    void set_reconfig(Vcpu_reconfiguration r) { _reconfig |= r; }
    bool tvm_enabled() { return static_cast<bool>(_tvm_requests.load()); }
    bool single_step_enabled() { return static_cast<bool>(_ss_requests.load()); }
    void switch_state_to_off();
    bool is_reconfig_needed(Vcpu_reconfiguration r) { return (_reconfig & r) != 0; }
    void unset_reconfig(Vcpu_reconfiguration r) { _reconfig &= ~r; }
    uint64 timer_offset() const { return _tmr_off; }
    void reset_interrupt_state() { _interrupt_state = NONE; }

public:
    // VCPU api start

    static bool init(uint16 vcpus);
    static void recall(Vcpu_id);
    static bool is_cpu_on(Vcpu_id);
    static void recall_all();
    static void resume_all();
    static Errno run(Vcpu_id);

    /*
     * Recall all vcpus except the one passed as an argument. This is useful
     * because this function is usually called in the context of a VM exit so
     * the current CPU is already stopped.
     */
    static void recall_all_but(Vcpu_id);
    static void reconfigure(Vcpu_id, Vcpu_reconfiguration r);
    static void reconfigure_all(Vcpu_reconfiguration r);
    static void reconfigure_all_but(Vcpu_id, Vcpu_reconfiguration r);

    static bool is_single_step_enabled_for_vcpu(Vcpu_id);
    static void ctrl_single_step(Vcpu_id cpu_id, bool enable,
                                 Request::Requestor requestor = Request::Requestor::REQUESTOR_VMM);
    static void ctrl_tvm(Vcpu_id cpu_id, bool enable,
                         Request::Requestor requestor = Request::Requestor::REQUESTOR_VMM,
                         const Reg_selection extra_regs = 0);
    static bool is_tvm_enabled(Vcpu_id);
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
    virtual void ctrl_tvm(bool enable,
                          Request::Requestor requestor = Request::Requestor::REQUESTOR_VMM,
                          const Reg_selection regs = 0)
        = 0;
    virtual void ctrl_single_step(bool enable,
                                  Request::Requestor requestor = Request::Requestor::REQUESTOR_VMM)
        = 0;

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