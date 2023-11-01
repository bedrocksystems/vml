/**
 * Copyright (C) 2019-2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/cpu_feature.hpp>
#include <model/vcpu_types.hpp>
#include <platform/atomic.hpp>
#include <platform/context.hpp>
#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/memory.hpp>
#include <platform/semaphore.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <platform/vm_types.hpp>

namespace Model {
    class Cpu_irq_interface;
    class Cpu;
    class Irq_controller;
    class Local_Irq_controller;
}

namespace Vbus {
    class Bus;
}

class Model::Cpu_irq_interface {
public:
    virtual void notify_interrupt_pending() = 0;
    virtual Model::Local_Irq_controller *local_irq_ctlr() = 0;
};

class Model::Cpu : public Model::Cpu_irq_interface {
public:
    enum Mode {
        // General mode
        BITS_64,
        BITS_32,
        BITS_16,
        // ARM Specific
        T32,
    };

    enum SpaceIdx {
        SIDX_GST = 0,

        // ARM only
        SIDX_NESTED = 1,

        // x86 only
        SIDX_PIO = 1,
        SIDX_MSR = 2,

        SIDX_MAX,
    };

    enum : unsigned { MAX_BOOT_ARGS = 4 };

private:
    Platform::Signal _resume_sig;
    Platform::Signal _off_sm;

    // Boot configuration
    uint64 _boot_addr{0};
    uint64 _boot_args[MAX_BOOT_ARGS] = {0, 0, 0, 0};
    uint64 _timer_offset{0};
    Mode _start_mode{BITS_64};

    Vcpu_id const _vcpu_id;

    Platform::Signal _irq_sig;

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
    enum State : uint8 {
        OFF = 0,
        OFF_ROUNDEDUP = 1,
        ON = 2,
        ON_ROUNDEDUP = 3,
        EMULATE = 4,
        EMULATE_ROUNDEDUP = 5,
    };

    atomic<State> _state{OFF};

    bool is_roundup_pending() const { return _state == EMULATE_ROUNDEDUP; }

    bool is_turned_on_by_guest() const { return !_execution_paused.is_requested_by(Request::Requestor::VMM); }

    void resume_vcpu() { _resume_sig.sig(); }
    void switch_on() { _off_sm.sig(); }
    void resume();

    void set_reset_parameters(uint64 boot_addr, uint64 const boot_arg[MAX_BOOT_ARGS], uint64 tmr_off, enum Mode m);

    static void roundup(Vcpu_id cpu_id) { get(cpu_id)->roundup_impl(); }

    bool block_timeout(uint64 const absolut_timeout) { return _irq_sig.wait(absolut_timeout); }
    void block() { _irq_sig.wait(); }
    void unblock() { _irq_sig.sig(); }
    void roundup_impl();
    void wait_if_exec_paused();

protected:
    Pcpu_id const _pcpu_id;
    Model::Irq_controller *const _girq_ctlr;
    Model::Local_Irq_controller *_lirq_ctlr{nullptr};

    void wait_for_switch_on() { _off_sm.wait(); }
    uint64 boot_addr() const { return _boot_addr; }
    const uint64 *boot_args() const { return _boot_args; }
    void switch_state_to_off();
    Mode start_mode() const { return _start_mode; }
    uint64 timer_offset() const { return _timer_offset; }

    CpuFeature _tvm;
    CpuFeature _hypercall;
    CpuFlag _reset;
    CpuFlag _single_step;
    CpuFlag _execution_paused;
    CpuFlag _icache_invalidate;
    CpuFeature _dump_regs;

    static Model::Cpu *get(Vcpu_id cpu_id);

public:
    // VCPU api start
    static bool init(uint16 vcpus);
    static void deinit();
    static bool is_cpu_turned_on_by_guest(Vcpu_id);
    static bool is_64bit(Vcpu_id);

    static uint16 get_num_vcpus();
    static Pcpu_id get_pcpu(Vcpu_id);
    static VcpuVHWId get_vcpu_vhw_id(Vcpu_id);

    // Debugging/Info purposes: describe the current status of the VCPU
    // NOTE: the specification of this function is simply: returns a string
    //       it isn't meaningful to specify what the string means.
    static const char *cpu_state_string(Vcpu_id);

    // Machine-level roundup logic
    static void roundup_all();
    static void resume_all();

    static void set_space_on(Vcpu_id id, RegAccessor &regs, Platform::Mem::MemSel space);

    // Configuration API
    typedef void (*ctrl_feature_cb)(Model::Cpu *, bool, Request::Requestor, Reg_selection);

    // note that the iterator functions do not access the state of the vcpu's directly, only the
    // callback functions access it.
    static void ctrl_feature_on_vcpu(ctrl_feature_cb cb, Vcpu_id, bool, Request::Requestor requestor = Request::Requestor::VMM,
                                     Reg_selection regs = 0);
    static void ctrl_feature_on_all_but_vcpu(ctrl_feature_cb cb, Vcpu_id, bool,
                                             Request::Requestor requestor = Request::Requestor::VMM, Reg_selection regs = 0);
    static void ctrl_feature_on_all_vcpus(ctrl_feature_cb cb, bool, Request::Requestor requestor = Request::Requestor::VMM,
                                          Reg_selection regs = 0);

    // these are the functions that are passed to the above functions for:
    // [ctrl_feature_cb].
    // there are more of these defined in subclasses, so this is some sort of extensible
    // enumeration that is represented extensionally.
    static void ctrl_feature_off(Model::Cpu *vcpu, bool enable, Request::Requestor requestor, Reg_selection regs);
    static void ctrl_feature_reset(Model::Cpu *vcpu, bool enable, Request::Requestor requestor, Reg_selection regs);
    static void ctrl_feature_tvm(Model::Cpu *vcpu, bool enable, Request::Requestor requestor, Reg_selection regs);
    static void ctrl_feature_single_step(Model::Cpu *vcpu, bool enable, Request::Requestor requestor, Reg_selection regs);
    static void ctrl_feature_icache_invalidate(Model::Cpu *vcpu, bool enable, Request::Requestor requestor, Reg_selection regs);
    static void ctrl_feature_hypercall(Model::Cpu *vcpu, bool enable, Request::Requestor requestor, Reg_selection regs);
    static void ctrl_feature_regs_dump(Model::Cpu *mcpu, bool enable, Request::Requestor requestor, Reg_selection regs);

    // this is just like the above but it requires threading an extra argument
    typedef void (*ctrl_feature_ex_cb)(Model::Cpu *, bool, Request::Requestor, uint64, Reg_selection);

    static void ctrl_register_trap_on_vcpu(ctrl_feature_ex_cb cb, Vcpu_id, bool, Request::Requestor, uint64, Reg_selection regs);

    // this is a [ctrl_feature_ex_cb]
    static void ctrl_register_trap_cb(Model::Cpu *vcpu, bool enable, Request::Requestor requestor, uint64 trap_id,
                                      Reg_selection regs);

    // Query API
    typedef bool (*requested_feature_cb)(Model::Cpu *, Request::Requestor);

    static bool is_feature_enabled_on_vcpu(requested_feature_cb cb, Vcpu_id,
                                           Request::Requestor requestor = Request::Requestor::VMM);

    // these are for the [cb] parameter of [is_feature_enabled_on_vcpu]
    static bool requested_feature_tvm(Model::Cpu *vcpu, Request::Requestor requestor);
    static bool requested_feature_single_step(Model::Cpu *vcpu, Request::Requestor requestor);
    static bool requested_feature_hypercall(Model::Cpu *vcpu, Request::Requestor requestor);
    static bool requested_feature_regs_dump(Model::Cpu *mcpu, Request::Requestor requestor);

    /*
     * Values are chosen to match the PSCI spec for conveniency. This could be changed
     * in the future if something else is needed.
     */
    enum StartErr { SUCCESS = 0, INVALID_PARAMETERS = -2, ALREADY_ON = -4, INVALID_ADDRESS = -9 };
    static StartErr start_cpu(Vcpu_id vcpu_id, Vbus::Bus &vbus, uint64 boot_addr, uint64 boot_args[MAX_BOOT_ARGS],
                              uint64 timer_off, enum Mode);
    static StartErr reset_cpu(Vcpu_id vcpu_id, uint64 boot_args[MAX_BOOT_ARGS], uint64 boot_addr, uint64 timer_off, enum Mode);

    // VCPU api end

    // Functions that should be provided by the implementation
    enum RecallReason : uint8 {
        IRQ,
        ROUNDUP,
        RECONFIG,
        MAX_REASONS,
    };
    virtual void recall(bool strong, RecallReason reason) = 0;

    // Functions that are implemented
    Cpu(Irq_controller *girq_ctlr, Vcpu_id vcpu_id, Pcpu_id pcpu_id);
    virtual ~Cpu();
    bool setup(const Platform_ctx *ctx);
    bool cleanup(const Platform_ctx *ctx);

    bool switch_state_to_roundedup();
    void switch_state_to_on();
    bool switch_state_to_emulating();

    Vcpu_id id() const { return _vcpu_id; }           // VMM ID, linear from 0 to N
    virtual VcpuVHWId vhw_id() const { return id(); } // virtual HW ID, can be non-linear

    // higher level API for CPU emulation
    void begin_emulation() {
        while (!switch_state_to_emulating()) {
            wait_for_resume();
        }
    }
    void end_emulation() { switch_state_to_on(); }

    void wait_for_resume() { _resume_sig.wait(); }
    void wait_for_interrupt(bool will_timeout, uint64 timeout_absolut);
    void notify_interrupt_pending() override;

    Model::Local_Irq_controller *local_irq_ctlr() override { return _lirq_ctlr; }

    virtual void ctrl_register_trap(bool, Request::Requestor, uint64, Reg_selection) {}

    virtual void set_vcpu_space(RegAccessor &, Platform::Mem::MemSel) {}
};
