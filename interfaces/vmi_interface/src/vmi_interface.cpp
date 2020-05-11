/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <model/cpu.hpp>
#include <types.hpp>
#include <vcpu/vcpu_roundup.hpp>
#include <vmi_interface/vmi_interface.hpp>

void
Vmm::Vcpu::ctrl_tvm(Vmm::Vcpu::Vcpu_id id, bool enable, uint64 regs) {
    Model::Cpu::ctrl_tvm(id, enable, Request::Requestor::REQUESTOR_VMI, regs);
}

uint16
Vmm::Vcpu::get_num_vcpus() {
    return Model::Cpu::get_num_vcpus();
}

Vmm::Vcpu::Cpu_id
Vmm::Vcpu::get_pcpu(Vmm::Vcpu::Vcpu_id id) {
    return Model::Cpu::get_pcpu(id);
}

void
Vmm::Vcpu::Roundup::roundup() {
    ::Vcpu::Roundup::roundup();
}

void
Vmm::Vcpu::Roundup::roundup_from_vcpu(Vmm::Vcpu::Vcpu_id vcpu_id) {
    ::Vcpu::Roundup::roundup_from_vcpu(vcpu_id);
}

void
Vmm::Vcpu::Roundup::resume() {
    ::Vcpu::Roundup::resume();
}
