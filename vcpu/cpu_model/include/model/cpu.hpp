/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/atomic.hpp>
#include <platform/context.hpp>
#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/semaphore.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <platform/vm_types.hpp>

namespace Model {
    class Cpu_irq_interface;
    class Cpu;
    class Cpu_feature;
    class Irq_controller;
    class Local_Irq_controller;
}

namespace Vbus {
    class Bus;
}

class Model::Cpu_irq_interface {
public:
    virtual void interrupt_pending() = 0;
    virtual Model::Local_Irq_controller *local_irq_ctlr() = 0;
};

namespace Request {
    enum Requestor : uint32 {
        VMM = 0,
        VMI = 1,
        MAX_REQUESTORS,
    };
}

class Model::Cpu_feature {
public:
    bool is_requested_by(Request::Requestor requestor) const {
        ASSERT(requestor < Request::MAX_REQUESTORS);
        return _requests[requestor] & ENABLE_MASK;
    }
    bool is_requested() const {
        return is_requested_by(Request::VMM) || is_requested_by(Request::VMI);
    }
    void force_reconfiguration() {
        uint64 v = _config_count;
        do {
            if (__UNLIKELY__(v == UINT64_MAX))
                return;
        } while (!_config_count.compare_exchange_weak(v, v + 1));
    }
    void set_config_gen() {
        _current_config = _config_count;
        /* Force the next round to reconfigure due to possible concurrent updates
         * to _config_count (leaving the value at UINT64_MAX). This will cause an extra
         * configuration that is not needed. But, this is okay and will happen very very
         * unfrequently (probably never?).
         */
        if (__UNLIKELY__(_current_config == UINT64_MAX)) {
            _config_count = 1;
            _current_config = 0;
        }
    }
    bool needs_reconfiguration() const { return _current_config != _config_count; }

    static constexpr uint8 ENABLE_SHIFT = 63;
    static constexpr uint64 ENABLE_MASK = 1ull << ENABLE_SHIFT;

    void get_current_config(bool &enabled, Reg_selection &regs) {
        uint64 vmm_conf = _requests[0];
        uint64 vmi_conf = _requests[1];

        enabled = (vmm_conf & ENABLE_MASK) || (vmi_conf & ENABLE_MASK);
        regs = (vmm_conf & ~ENABLE_MASK) | (vmi_conf & ~ENABLE_MASK);

        if (!enabled)
            regs = 0; // Force no register when the feature is not enabled
    }

    // Each requestor is responsible of maintaining the consistency of its config
    void request(bool enable, Request::Requestor requestor, Reg_selection regs = 0) {
        ASSERT(requestor < Request::MAX_REQUESTORS);
        ASSERT(!(ENABLE_MASK & regs)); // Reg_selection doesn't use the highest bit for now
        _requests[requestor] = ((enable ? 1ull : 0ull) << ENABLE_SHIFT) | regs;
        force_reconfiguration();
    }

private:
    atomic<uint64> _requests[Request::MAX_REQUESTORS] = {0ull, 0ull};
    atomic<uint64> _config_count{0};
    uint64 _current_config{0};
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

    static constexpr unsigned MAX_BOOT_ARGS = 4;

private:
    Platform::Signal _resume_sig;
    Semaphore _off_sm;
    uint64 _boot_addr{0};
    uint64 _boot_args[MAX_BOOT_ARGS] = {0, 0, 0, 0};

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

    void resume_vcpu() { _resume_sig.sig(); }
    void switch_on() { _off_sm.release(); }
    void resume();

    void set_reset_parameters(uint64 boot_addr, uint64 const boot_arg[MAX_BOOT_ARGS],
                              uint64 tmr_off, enum Mode m);

    virtual void ctrl_tvm(bool enable, Request::Requestor requestor = Request::Requestor::VMM,
                          Reg_selection regs = 0)
        = 0;
    virtual void ctrl_single_step(bool enable,
                                  Request::Requestor requestor = Request::Requestor::VMM)
        = 0;

    static void roundup(Vcpu_id);

    bool block_timeout(uint64 const absolut_timeout) { return _irq_sig.wait(absolut_timeout); }
    void block() { _irq_sig.wait(); }
    void unblock() { _irq_sig.sig(); }
    void roundup_impl();

protected:
    Mode _start_mode{BITS_64};
    uint64 _tmr_off{0};

    Pcpu_id const _pcpu_id;
    Model::Irq_controller *const _girq_ctlr;
    Model::Local_Irq_controller *_lirq_ctlr{nullptr};

    void wait_for_switch_on() { _off_sm.acquire(); }
    uint64 boot_addr() const { return _boot_addr; }
    const uint64 *boot_args() const { return _boot_args; }
    void switch_state_to_off();
    uint64 timer_offset() const { return _tmr_off; }

    Cpu_feature _reset;
    Cpu_feature _tvm;
    Cpu_feature _singe_step;
    Cpu_feature _execution_paused;
    Cpu_feature _icache_invalidate;

public:
    // VCPU api start

    static bool init(uint16 vcpus);
    static bool is_cpu_turned_on_by_guest(Vcpu_id);
    static void roundup_all();
    static void resume_all();
    static bool is_64bit(Vcpu_id);
    static Errno run(Vcpu_id);

    // Debugging/Info purposes: describe the current status of the VCPU
    static const char *cpu_state_string(Vcpu_id);

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
    static void ctrl_feature_icache_invalidate(Model::Cpu *vcpu, bool enable,
                                               Request::Requestor requestor, Reg_selection regs);
    static bool requested_feature_tvm(Model::Cpu *vcpu, Request::Requestor requestor);
    static bool requested_feature_single_step(Model::Cpu *vcpu, Request::Requestor requestor);

    static uint16 get_num_vcpus();
    static Pcpu_id get_pcpu(Vcpu_id);

    /*
     * Values are chosen to match the PSCI spec for conveniency. This could be changed
     * in the future if something else is needed.
     */
    enum StartErr { SUCCESS = 0, INVALID_PARAMETERS = -2, ALREADY_ON = -4, INVALID_ADDRESS = -9 };
    static StartErr start_cpu(Vcpu_id vcpu_id, Vbus::Bus &vbus, uint64 boot_addr,
                              uint64 boot_args[MAX_BOOT_ARGS], uint64 timer_off, enum Mode);
    static StartErr reset_cpu(Vcpu_id vcpu_id, uint64 boot_args[MAX_BOOT_ARGS], uint64 boot_addr,
                              uint64 timer_off, enum Mode);

    // VCPU api end

    // Functions that should be provided by the implementation
    virtual bool recall(bool strong) = 0;
    virtual Errno run() = 0;

    // Functions that are implemented
    Cpu(Irq_controller *girq_ctlr, Vcpu_id vcpu_id, Pcpu_id pcpu_id);
    bool setup(const Platform_ctx *ctx);

    bool switch_state_to_roundedup();
    void switch_state_to_on();
    bool switch_state_to_emulating();

    Vcpu_id id() const { return _vcpu_id; }

    void wait_for_resume() { _resume_sig.wait(); }
    void wait_for_interrupt(bool will_timeout, uint64 timeout_absolut);
    void interrupt_pending() override;

    virtual Model::Local_Irq_controller *local_irq_ctlr() override { return _lirq_ctlr; }
};
