/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <arch/barrier.hpp>
#include <debug_switches.hpp>
#include <model/cpu.hpp>
#include <model/gic.hpp>
#include <model/timer.hpp>
#include <model/vcpu_types.hpp>
#include <platform/atomic.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>
#include <vcpu/vcpu_roundup.hpp>

namespace Model {
    static uint16 configured_vcpus;
    static atomic<uint64> num_vcpus;
    static Cpu** vcpus;
}

const char* state_printable_name[]
    = {"OFF", "OFF_ROUNDEDUP", "ON", "ON_ROUNDEDUP", "EMULATE", "EMULATE_ROUNDEDUP"};

bool
Model::Cpu::init(uint16 config_vcpus) {
    configured_vcpus = config_vcpus;
    vcpus = new (nothrow) Model::Cpu*[configured_vcpus];
    if (vcpus == nullptr)
        return false;

    return true;
}

bool
Model::Cpu::is_64bit(Vcpu_id id) {
    return vcpus[id]->_start_mode == AA64;
}

uint16
Model::Cpu::get_num_vcpus() {
    return static_cast<uint16>(num_vcpus);
}

Pcpu_id
Model::Cpu::get_pcpu(Vcpu_id id) {
    ASSERT(id < num_vcpus);
    return vcpus[id]->_pcpu_id;
}

void
Model::Cpu::roundup(Vcpu_id cpu_id) {
    ASSERT(cpu_id < num_vcpus);

    vcpus[cpu_id]->switch_state_to_roundedup();
    vcpus[cpu_id]->recall();
    vcpus[cpu_id]->unblock(); // If the VCPU is in WFI, unblock it
}

void
Model::Cpu::roundup_all() {
    for (Vcpu_id i = 0; i < num_vcpus; ++i)
        roundup(i);
}

void
Model::Cpu::resume_all() {
    for (Vcpu_id i = 0; i < num_vcpus; ++i)
        vcpus[i]->resume();
}

void
Model::Cpu::ctrl_feature_on_vcpu(ctrl_feature_cb cb, Vcpu_id vcpu_id, bool enabled,
                                 Request::Requestor requestor, Reg_selection regs) {
    ASSERT(vcpu_id < num_vcpus);

    Model::Cpu* vcpu = vcpus[vcpu_id];
    cb(vcpu, enabled, requestor, regs);
}

void
Model::Cpu::ctrl_feature_on_all_but_vcpu(ctrl_feature_cb cb, Vcpu_id id, bool enabled,
                                         Request::Requestor requestor, Reg_selection regs) {
    ASSERT(id < num_vcpus);
    for (Vcpu_id i = 0; i < num_vcpus; ++i) {
        Model::Cpu* vcpu = vcpus[i];

        if (i != id)
            cb(vcpu, enabled, requestor, regs);
    }
}

void
Model::Cpu::ctrl_feature_on_all_vcpus(ctrl_feature_cb cb, bool enabled,
                                      Request::Requestor requestor, Reg_selection regs) {
    for (Vcpu_id i = 0; i < num_vcpus; ++i) {
        Model::Cpu* vcpu = vcpus[i];

        cb(vcpu, enabled, requestor, regs);
    }
}

bool
Model::Cpu::is_feature_enabled_on_vcpu(requested_feature_cb cb, Vcpu_id vcpu_id,
                                       Request::Requestor requestor) {
    ASSERT(vcpu_id < num_vcpus);

    Model::Cpu* vcpu = vcpus[vcpu_id];
    return cb(vcpu, requestor);
}

void
Model::Cpu::ctrl_feature_tvm(Model::Cpu* vcpu, bool enable, Request::Requestor requestor,
                             Reg_selection regs) {
    vcpu->ctrl_tvm(enable, requestor, regs);
}

void
Model::Cpu::ctrl_feature_single_step(Model::Cpu* vcpu, bool enable, Request::Requestor requestor,
                                     Reg_selection) {
    vcpu->ctrl_single_step(enable, requestor);
}

bool
Model::Cpu::requested_feature_tvm(Model::Cpu* vcpu, Request::Requestor requestor) {
    return vcpu->_tvm.is_requested_by(requestor);
}

bool
Model::Cpu::requested_feature_single_step(Model::Cpu* vcpu, Request::Requestor requestor) {
    return vcpu->_singe_step.is_requested_by(requestor);
}

void
Model::Cpu::ctrl_feature_off(Model::Cpu* vcpu, bool enable, Request::Requestor requestor,
                             Reg_selection) {
    vcpu->_execution_paused.request(enable, requestor);
    if (!enable) {
        vcpu->switch_on();
    } else {
        /* A VCPU is switched off at the beginning of the VMExit handler so issuing a
         * recall is a more robust approach as it will guarantee that the VCPU will not
         * progress any more after that call.
         */
        vcpu->recall();
    }
}

void
Model::Cpu::ctrl_feature_reset(Model::Cpu* vcpu, bool enable, Request::Requestor requestor,
                               Reg_selection) {
    vcpu->_reset.request(enable, requestor);
}

void
Model::Cpu::ctrl_feature_icache_invalidate(Model::Cpu* vcpu, bool enable,
                                           Request::Requestor requestor, Reg_selection) {
    vcpu->_icache_invalidate.request(enable, requestor);
}

Errno
Model::Cpu::run(Vcpu_id cpu_id) {
    ASSERT(cpu_id < num_vcpus);

    return vcpus[cpu_id]->run();
}

Model::Cpu::Start_err
Model::Cpu::start_cpu(Vcpu_id vcpu_id, Vbus::Bus& vbus, uint64 boot_addr,
                      uint64 boot_args[MAX_BOOT_ARGS], uint64 timer_off, enum Mode m) {
    if (vcpu_id >= num_vcpus) {
        WARN("vCPU " FMTu64 " number out of bound", vcpu_id);
        return INVALID_PARAMETERS;
    }

    if (is_cpu_turned_on_by_guest(vcpu_id)) {
        WARN("Trying to power on VCPU " FMTu64 " but it is already on", vcpu_id);
        return ALREADY_ON;
    }

    Vbus::Device* target_ram = vbus.get_device_at(boot_addr, 1);

    if (target_ram == nullptr
        || (target_ram->type() != Vbus::Device::GUEST_PHYSICAL_STATIC_MEMORY
            && target_ram->type() != Vbus::Device::GUEST_PHYSICAL_DYNAMIC_MEMORY)) {
        WARN(FMTx64 " is not a valid boot address", boot_addr)
        return INVALID_ADDRESS;
    }

    Model::Cpu* const vcpu = vcpus[vcpu_id];

    vcpu->set_reset_parameters(boot_addr, boot_args, timer_off, m);
    Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_reset, vcpu_id, true);
    Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_off, vcpu_id, false);
    return SUCCESS;
}

bool
Model::Cpu::is_cpu_turned_on_by_guest(Vcpu_id cpu_id) {
    if (cpu_id >= num_vcpus)
        return false;

    return vcpus[cpu_id]->is_turned_on_by_guest();
}

Model::Cpu::Cpu(Gic_d& gic, Vcpu_id vcpu_id, Pcpu_id pcpu_id, uint16 const irq)
    : _vcpu_id(vcpu_id), _timer_irq(irq), _pcpu_id(pcpu_id), _gic(&gic) {
    gic.enable_cpu(this, _vcpu_id);
    vcpus[vcpu_id] = this;
    Barrier::w_before_w();
    num_vcpus++;
}

bool
Model::Cpu::setup(const Platform_ctx* ctx) {
    bool ok = _off_sm.init(ctx);
    if (!ok)
        return false;

    ok = _resume_sm.init(ctx);
    if (!ok)
        return false;

    _gic_r = new (nothrow) Model::Gic_r(*_gic, _vcpu_id, _vcpu_id == configured_vcpus - 1u);
    if (_gic_r == nullptr)
        return false;

    _timer = new (nothrow) Model::Timer(*_gic, _vcpu_id, _timer_irq);
    if (_timer == nullptr) {
        delete _gic_r;
        return false;
    }

    ok = _timer->init_irq(_vcpu_id, _timer_irq, _timer_irq, true);
    if (!ok) {
        delete _timer;
        delete _gic_r;
        return false;
    }

    return true;
}

/*! \brief Request the VCPU to round (i.e. stop its progress)
 *
 * If the VCPU was in ON or OFF, the strong recall from NOVA already guarantees
 * that it will stop progressing and call the 'recall' portal. We declare them as
 * 'done progressing' right away. The only exception is CPUs that are emulating:
 * we need to wait for them to finish. They will signal themselves later on.
 */
void
Model::Cpu::switch_state_to_roundedup() {
    enum State new_state, cur_state;

    do {
        cur_state = new_state = _state;
        switch (cur_state) {
        case ON:
            new_state = ON_ROUNDEDUP;
            break;
        case OFF:
            new_state = OFF_ROUNDEDUP;
            break;
        case EMULATE:
            new_state = EMULATE_ROUNDEDUP;
            break;
        case OFF_ROUNDEDUP:
        case ON_ROUNDEDUP:
        case EMULATE_ROUNDEDUP:
            ABORT_WITH("Unexpected state for VCPU " FMTu64 ": %s", id(),
                       state_printable_name[cur_state]);
        }
    } while (!_state.cas(cur_state, new_state));

    if (__UNLIKELY__(Debug::current_level == Debug::Full))
        INFO("VCPU " FMTu64 " state %s -> %s", id(), state_printable_name[cur_state],
             state_printable_name[new_state]);

    if (cur_state != EMULATE)
        Vcpu::Roundup::vcpu_notify_done_progessing();
}

void
Model::Cpu::resume() {
    enum State new_state, cur_state = _state;

    do {
        cur_state = new_state = _state;
        switch (cur_state) {
        case OFF_ROUNDEDUP:
            new_state = OFF;
            break;
        case ON_ROUNDEDUP:
            new_state = ON;
            break;
        case EMULATE_ROUNDEDUP:
            new_state = EMULATE;
            break;
        default:
            ABORT_WITH("Unexpected state for VCPU " FMTu64 ": %s", id(),
                       state_printable_name[cur_state]);
        }
    } while (!_state.cas(cur_state, new_state));

    if (__UNLIKELY__(Debug::current_level == Debug::Full))
        INFO("VCPU " FMTu64 " state %s -> %s", id(), state_printable_name[cur_state],
             state_printable_name[new_state]);

    resume_vcpu();
}

/*! \brief Switch the VCPU state to ON (i.e. not emulating)
 *
 * This is called in two cases: a VCPU is turned on by the guest or a VCPU is done
 * emulating. If the VCPU was emulating and a roundup was waiting for it to finish,
 * it will notify the roundup code via 'done_progressing'.
 */
void
Model::Cpu::switch_state_to_on() {
    enum State new_state, cur_state = _state;

    do {
        cur_state = new_state = _state;
        switch (cur_state) {
        case OFF_ROUNDEDUP:
        case EMULATE_ROUNDEDUP:
            new_state = ON_ROUNDEDUP;
            break;
        case OFF:
        case EMULATE:
            new_state = ON;
            break;
        default:
            ABORT_WITH("Unexpected state for VCPU " FMTu64 ": %s", id(),
                       state_printable_name[cur_state]);
        }
    } while (!_state.cas(cur_state, new_state));

    if (cur_state == EMULATE_ROUNDEDUP)
        Vcpu::Roundup::vcpu_notify_done_progessing();

    if (cur_state == OFF || cur_state == OFF_ROUNDEDUP)
        Vcpu::Roundup::vcpu_notify_switched_on();

    if (__UNLIKELY__(Debug::current_level == Debug::Full))
        INFO("VCPU " FMTu64 " state %s -> %s", id(), state_printable_name[cur_state],
             state_printable_name[new_state]);
}

void
Model::Cpu::switch_state_to_off() {
    enum State new_state, cur_state;

    do {
        cur_state = new_state = _state;
        switch (cur_state) {
        case ON_ROUNDEDUP:
            new_state = OFF_ROUNDEDUP;
            break;
        case ON:
            new_state = OFF;
            break;
        default:
            ABORT_WITH("Unexpected state for VCPU " FMTu64 ": %s", id(),
                       state_printable_name[cur_state]);
        }
    } while (!_state.cas(cur_state, new_state));

    if (__UNLIKELY__(Debug::current_level == Debug::Full))
        INFO("VCPU " FMTu64 " state %s -> %s", id(), state_printable_name[cur_state],
             state_printable_name[new_state]);
}

/*! \brief Enter an emulation section in the VMM. This may fail.
 *
 * This is an interesting function with two cases:
 * - There is no roundup going on, we are clear to emulate. If a roundup comes in after us,
 * it will have to wait for us to finish.
 * - A roundup is taking place, we are not allowed to enter emulation. The caller will
 * have to wait and try again.
 *
 * \return true if emulation can be started, false otherwise.
 */
bool
Model::Cpu::switch_state_to_emulating() {
    enum State new_state, cur_state;

    while (_execution_paused.is_requested()) {
        switch_state_to_off();
        wait_for_switch_on();
        switch_state_to_on();
    }

    do {
        cur_state = new_state = _state;
        switch (cur_state) {
        case ON_ROUNDEDUP:
            return false;
        case ON:
            new_state = EMULATE;
            break;
        default:
            ABORT_WITH("Unexpected state for VCPU " FMTu64 ": %s", id(),
                       state_printable_name[cur_state]);
        }
    } while (!_state.cas(cur_state, new_state));

    if (__UNLIKELY__(Debug::current_level == Debug::Full))
        INFO("VCPU " FMTu64 " state %s -> %s", id(), state_printable_name[cur_state],
             state_printable_name[new_state]);

    return true;
}

uint8
Model::Cpu::aff0() const {
    return _gic_r->aff0();
}
uint8
Model::Cpu::aff1() const {
    return _gic_r->aff1();
}
uint8
Model::Cpu::aff2() const {
    return _gic_r->aff2();
}
uint8
Model::Cpu::aff3() const {
    return _gic_r->aff3();
}

void
Model::Cpu::assert_vtimer(uint64 const control) {
    _timer->assert_irq(control);
}

void
Model::Cpu::wait_for_interrupt(uint64 const control, uint64 const timeout_absolut) {
    Interrupt_state expected = NONE;
    if (_interrupt_state.cas(expected, SLEEPING)) {
        bool will_timeout = _timer->schedule_timeout(control, timeout_absolut, this);
        if (!will_timeout)
            block();

        expected = SLEEPING;
    } else
        expected = PENDING;

    _interrupt_state.cas(expected, NONE);
}

void
Model::Cpu::interrupt_pending() {
    recall();

    Interrupt_state expected = (_interrupt_state == PENDING) ? PENDING : NONE;

    if (!_interrupt_state.cas(expected, PENDING)) {
        expected = SLEEPING;
        _interrupt_state.cas(expected, NONE);

        unblock();
    }
}

bool
Model::Cpu::pending_irq(uint64& lr) {
    Gic_d::Lr lrc(lr);

    bool res = _gic->pending_irq(_vcpu_id, lrc);
    lr = lrc.value();
    return res;
}

void
Model::Cpu::set_reset_parameters(uint64 const boot_addr, uint64 const boot_args[MAX_BOOT_ARGS],
                                 uint64 const tmr_off, enum Mode m) {
    _boot_addr = boot_addr;
    for (unsigned i = 0; i < MAX_BOOT_ARGS; i++)
        _boot_args[i] = boot_args[i];
    _tmr_off = tmr_off;
    _start_mode = m;
    Barrier::rw_before_rw();
}
