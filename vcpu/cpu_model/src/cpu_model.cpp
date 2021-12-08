/**
 * Copyright (C) 2019-2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <arch/barrier.hpp>
#include <debug_switches.hpp>
#include <model/cpu.hpp>
#include <model/irq_controller.hpp>
#include <model/vcpu_types.hpp>
#include <platform/atomic.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>
#include <vcpu/vcpu_roundup.hpp>

namespace Model {
    static uint16 configured_vcpus;
    static Cpu** vcpus;
}

const char* state_printable_name[]
    = {"OFF", "OFF_ROUNDEDUP", "ON", "ON_ROUNDEDUP", "EMULATE", "EMULATE_ROUNDEDUP"};

bool
Model::Cpu::init(uint16 config_vcpus) {
    configured_vcpus = config_vcpus;
    vcpus = new (nothrow) Model::Cpu*[configured_vcpus];

    return vcpus != nullptr;
}

bool
Model::Cpu::is_64bit(Vcpu_id id) {
    return vcpus[id]->_start_mode == BITS_64;
}

uint16
Model::Cpu::get_num_vcpus() {
    return configured_vcpus;
}

Pcpu_id
Model::Cpu::get_pcpu(Vcpu_id id) {
    ASSERT(id < configured_vcpus);
    return vcpus[id]->_pcpu_id;
}

const char*
Model::Cpu::cpu_state_string(Vcpu_id id) {
    ASSERT(id < configured_vcpus);
    return state_printable_name[vcpus[id]->_state];
}

void
Model::Cpu::roundup_impl() {
    bool emulating = switch_state_to_roundedup();
    recall(true);
    unblock(); // If the VCPU is in WFI, unblock it
    if (!emulating)
        Vcpu::Roundup::vcpu_notify_done_progressing();
}

// is this function still needed?
void
Model::Cpu::roundup(Vcpu_id cpu_id) {
    ASSERT(cpu_id < configured_vcpus);
    vcpus[cpu_id]->roundup_impl();
}

void
Model::Cpu::roundup_all() {
    uint64 max = configured_vcpus;
    for (Vcpu_id i = 0; i < max; ++i)
        vcpus[i]->roundup_impl();
}

void
Model::Cpu::resume_all() {
    uint64 max = configured_vcpus;
    for (Vcpu_id i = 0; i < max; ++i)
        vcpus[i]->resume();
}

void
Model::Cpu::ctrl_feature_on_vcpu(ctrl_feature_cb cb, Vcpu_id vcpu_id, bool enabled,
                                 Request::Requestor requestor, Reg_selection regs) {
    ASSERT(vcpu_id < configured_vcpus);

    Model::Cpu* vcpu = vcpus[vcpu_id];
    cb(vcpu, enabled, requestor, regs);
}

void
Model::Cpu::ctrl_feature_on_all_but_vcpu(ctrl_feature_cb cb, Vcpu_id id, bool enabled,
                                         Request::Requestor requestor, Reg_selection regs) {
    ASSERT(id < configured_vcpus);
    for (Vcpu_id i = 0; i < configured_vcpus; ++i) {
        Model::Cpu* vcpu = vcpus[i];

        if (i != id)
            cb(vcpu, enabled, requestor, regs);
    }
}

void
Model::Cpu::ctrl_feature_on_all_vcpus(ctrl_feature_cb cb, bool enabled,
                                      Request::Requestor requestor, Reg_selection regs) {
    for (Vcpu_id i = 0; i < configured_vcpus; ++i) {
        Model::Cpu* vcpu = vcpus[i];

        cb(vcpu, enabled, requestor, regs);
    }
}

bool
Model::Cpu::is_feature_enabled_on_vcpu(requested_feature_cb cb, Vcpu_id vcpu_id,
                                       Request::Requestor requestor) {
    ASSERT(vcpu_id < configured_vcpus);

    Model::Cpu* vcpu = vcpus[vcpu_id];
    return cb(vcpu, requestor);
}

void
Model::Cpu::ctrl_feature_tvm(Model::Cpu* vcpu, bool enable, Request::Requestor requestor,
                             Reg_selection regs) {
    ASSERT(vcpu != nullptr);
    vcpu->_tvm.request(enable, requestor, regs);
}

void
Model::Cpu::ctrl_feature_single_step(Model::Cpu* vcpu, bool enable, Request::Requestor requestor,
                                     Reg_selection) {
    ASSERT(vcpu != nullptr);
    vcpu->_singe_step.request(enable, requestor);
}

bool
Model::Cpu::requested_feature_tvm(Model::Cpu* vcpu, Request::Requestor requestor) {
    return vcpu->_tvm.is_requested_by(requestor);
}

bool
Model::Cpu::requested_feature_single_step(Model::Cpu* vcpu, Request::Requestor requestor) {
    return vcpu->_singe_step.is_requested_by(requestor);
}

bool
Model::Cpu::requested_feature_hypercall(Model::Cpu* vcpu, Request::Requestor requestor) {
    return vcpu->_hypercall.is_requested_by(requestor);
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
        vcpu->recall(true);
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

void
Model::Cpu::ctrl_feature_hypercall(Model::Cpu* vcpu, bool enable, Request::Requestor requestor,
                                   Reg_selection regs) {
    vcpu->_hypercall.request(enable, requestor, regs);
}

Errno
Model::Cpu::run(Vcpu_id cpu_id) {
    ASSERT(cpu_id < configured_vcpus);

    return vcpus[cpu_id]->run();
}

Model::Cpu::StartErr
Model::Cpu::start_cpu(Vcpu_id vcpu_id, Vbus::Bus& vbus, uint64 boot_addr,
                      uint64 boot_args[MAX_BOOT_ARGS], uint64 timer_off, enum Mode m) {
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

    return reset_cpu(vcpu_id, boot_args, boot_addr, timer_off, m);
}

Model::Cpu::StartErr
Model::Cpu::reset_cpu(Vcpu_id vcpu_id, uint64 boot_args[MAX_BOOT_ARGS], uint64 boot_addr,
                      uint64 timer_off, enum Mode m) {
    if (vcpu_id >= configured_vcpus) {
        WARN("vCPU " FMTu64 " number out of bound", vcpu_id);
        return INVALID_PARAMETERS;
    }

    Model::Cpu* const vcpu = vcpus[vcpu_id];
    ASSERT(vcpu);

    vcpu->set_reset_parameters(boot_addr, boot_args, timer_off, m);
    Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_reset, vcpu_id, true);
    Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_off, vcpu_id, false);
    return SUCCESS;
}

bool
Model::Cpu::is_cpu_turned_on_by_guest(Vcpu_id cpu_id) {
    if (cpu_id >= configured_vcpus)
        return false;

    return vcpus[cpu_id]->is_turned_on_by_guest();
}

Model::Cpu::Cpu(Irq_controller* girq_ctlr, Vcpu_id vcpu_id, Pcpu_id pcpu_id)
    : _vcpu_id(vcpu_id), _pcpu_id(pcpu_id), _girq_ctlr(girq_ctlr) {
    _girq_ctlr->enable_cpu(this, _vcpu_id);
    vcpus[vcpu_id] = this;
    Barrier::w_before_w();
}

bool
Model::Cpu::setup(const Platform_ctx* ctx) {
    bool ok = _off_sm.init(ctx);
    if (!ok)
        return false;

    ok = _resume_sig.init(ctx);
    if (!ok)
        return false;

    return _irq_sig.init(ctx);
}

/*! \brief Request the VCPU to round (i.e. stop its progress)
 *
 * If the VCPU was in ON or OFF, the strong recall from NOVA already guarantees
 * that it will stop progressing and call the 'recall' portal. We declare them as
 * 'done progressing' right away. The only exception is CPUs that are emulating:
 * we need to wait for them to finish. They will signal themselves later on.
 */
bool
Model::Cpu::switch_state_to_roundedup() {
    enum State new_state, cur_state = _state;

    do {
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
        default:
            // case OFF_ROUNDEDUP:
            // case ON_ROUNDEDUP:
            // case EMULATE_ROUNDEDUP:
            ABORT_WITH("Unexpected state for VCPU " FMTu64 ": %s", id(),
                       state_printable_name[cur_state]);
        }
    } while (!_state.cas(cur_state, new_state));

    if (__UNLIKELY__(Debug::current_level == Debug::FULL))
        INFO("VCPU " FMTu64 " state %s -> %s", id(), state_printable_name[cur_state],
             state_printable_name[new_state]);

    return (cur_state == EMULATE);
}

void
Model::Cpu::resume() {
    enum State new_state, cur_state = _state;

    do {
        switch (cur_state) {
        case ON_ROUNDEDUP:
            new_state = ON;
            break;
        case OFF_ROUNDEDUP:
            new_state = OFF;
            break;
        case EMULATE_ROUNDEDUP:
            new_state = EMULATE;
            break;
        default:
            ABORT_WITH("Unexpected state for VCPU " FMTu64 ": %s", id(),
                       state_printable_name[cur_state]);
        }
    } while (!_state.cas(cur_state, new_state));

    if (__UNLIKELY__(Debug::current_level == Debug::FULL))
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
        Vcpu::Roundup::vcpu_notify_done_progressing();

    if (cur_state == OFF || cur_state == OFF_ROUNDEDUP)
        Vcpu::Roundup::vcpu_notify_switched_on();

    if (__UNLIKELY__(Debug::current_level == Debug::FULL))
        INFO("VCPU " FMTu64 " state %s -> %s", id(), state_printable_name[cur_state],
             state_printable_name[new_state]);
}

void
Model::Cpu::switch_state_to_off() {
    enum State new_state, cur_state = _state;

    do {
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

    if (__UNLIKELY__(Debug::current_level == Debug::FULL))
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
    while (_execution_paused.is_requested()) {
        switch_state_to_off();
        wait_for_switch_on();
        switch_state_to_on();
    }

    enum State new_state, cur_state = _state;

    do {
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

    if (__UNLIKELY__(Debug::current_level == Debug::FULL))
        INFO("VCPU " FMTu64 " state %s -> %s", id(), state_printable_name[cur_state],
             state_printable_name[new_state]);

    return true;
}

void
Model::Cpu::wait_for_interrupt(bool will_timeout, uint64 const timeout_absolut) {
    if (!will_timeout)
        while (!_lirq_ctlr->int_pending() && !_lirq_ctlr->nmi_pending())
            block();
    else {
        if (!_lirq_ctlr->int_pending())
            block_timeout(timeout_absolut);
    }
}

void
Model::Cpu::wait_for_interrupt(Vcpu_id vcpu_id, uint64 control, uint64 const timeout_absolut) {
    ASSERT(vcpu_id < configured_vcpus);

    Model::Cpu* vcpu = vcpus[vcpu_id];
    vcpu->wait_for_interrupt(control, timeout_absolut);
}

void
Model::Cpu::interrupt_pending() {
    recall(false);
    unblock();
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
