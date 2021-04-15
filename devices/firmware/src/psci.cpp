/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <lifecycle.hpp>
#include <model/cpu.hpp>
#include <model/psci.hpp>
#include <model/simple_as.hpp>
#include <msr/msr_info.hpp>
#include <platform/context.hpp>
#include <platform/log.hpp>
#include <platform/reg_accessor.hpp>
#include <vcpu/vcpu_roundup.hpp>

static constexpr uint32 MAJOR_VERSION = 0x1u << 16;
static constexpr uint32 MINOR_VERSION = 0x1u;
static constexpr uint32 SMCCC_MAJOR_VERSION = 0x1u << 16;
static constexpr uint32 SMCCC_MINOR_VERSION = 0x1u;

enum Function_id : uint32 {
    SMCCC_VERSION = 0x80000000u,
    SMCCC_ARCH_FEATURES = 0x80000001u,
    VERSION = 0x84000000u,
    FEATURES = 0x8400000au,
    CPU_SUSPEND_32 = 0x8400000eu,
    CPU_SUSPEND_64 = 0xc400000eu,
    CPU_OFF = 0x84000002u,
    AFFINITY_INFO_32 = 0x84000004u,
    AFFINITY_INFO_64 = 0xc4000004u,
    MIGRATE_INFO_TYPE = 0x84000006u,
    CPU_ON_32 = 0x84000003u,
    CPU_ON_64 = 0xC4000003u,
    SYSTEM_OFF = 0x84000008,
    SYSTEM_RESET = 0x84000009,
};

enum Result : int32 {
    SUCCESS = 0,
    NOT_SUPPORTED = -1,
    INVALID_PARAMETERS = -2,
    DENIED = -3,
    ALREADY_ON = -4,
    ON_PENDING = -5,
    INTERNAL_FAILURE = -6,
    NOT_PRESENT = -7,
    DISABLED = -8,
    INVALID_ADDRESS = -9
};

static constexpr uint32
decode_cpu_id(uint64 arg) {
    return static_cast<uint32>(arg | ((arg >> 8) & 0xffull << 24));
}

bool
Firmware::Psci::smc_call_service(const VcpuCtx &vctx, RegAccessor &arch, Vbus::Bus &vbus,
                                 uint64 const function_id, uint64 &res) {
    switch (static_cast<Function_id>(function_id)) {
    case SMCCC_VERSION:
        res = static_cast<uint64>(SMCCC_MAJOR_VERSION | SMCCC_MINOR_VERSION);
        return true;
    case SMCCC_ARCH_FEATURES:
        res = static_cast<uint64>(NOT_SUPPORTED); /*only version discovery is supported*/
        return true;
    case VERSION:
        res = static_cast<uint64>(MAJOR_VERSION | MINOR_VERSION);
        return true;
    case FEATURES: {
        uint64 feature = arch.gpr(1);
        if (feature == CPU_SUSPEND_64 || feature == CPU_SUSPEND_32)
            res = 0; // Function is present but not supported
        else if (feature == VERSION || feature == CPU_ON_32 || feature == CPU_ON_64
                 || feature == AFFINITY_INFO_64 || feature == AFFINITY_INFO_32 || feature == CPU_OFF
                 || feature == SYSTEM_OFF || feature == SYSTEM_RESET || feature == SMCCC_VERSION)
            res = 0; // The function is present
        else
            res = static_cast<uint64>(NOT_SUPPORTED);

        return true;
    }
    case MIGRATE_INFO_TYPE:
        res = 2; /* no migration */
        return true;
    case CPU_ON_64:
    case CPU_ON_32: {
        enum Model::Cpu::Start_err err;
        Msr::Info::Spsr spsr(arch.el2_spsr());
        enum Model::Cpu::Mode mode;
        uint64 boot_addr = arch.gpr(2);
        uint64 boot_args[Model::Cpu::MAX_BOOT_ARGS] = {arch.gpr(3), 0, 0, 0};

        if (spsr.is_aa32()) {
            if (boot_addr & 0x1ull) {
                mode = Model::Cpu::T32;
                boot_addr = boot_addr & ~0x1ull;
            } else {
                mode = Model::Cpu::AA32;
            }
        } else {
            mode = Model::Cpu::AA64;
        }

        err = Model::Cpu::start_cpu(decode_cpu_id(arch.gpr(1)), vbus, boot_addr, boot_args,
                                    arch.tmr_cntvoff(), mode);
        res = static_cast<uint64>(err);
        return true;
    }
    case CPU_SUSPEND_32:
    case CPU_SUSPEND_64:
        DEBUG("Request to suspend to CPU, but, this function is not supported.");
        res = static_cast<uint64>(NOT_SUPPORTED);
        return true;
    case AFFINITY_INFO_32:
    case AFFINITY_INFO_64: {
        uint32 vcpu_id = decode_cpu_id(arch.gpr(1));
        uint64 aff_level = arch.gpr(2);

        if (aff_level != 0)
            res = static_cast<uint64>(INVALID_PARAMETERS); // No need to implement this with 1.0
        if (vcpu_id > Model::Cpu::get_num_vcpus())
            res = static_cast<uint64>(INVALID_PARAMETERS);

        if (Model::Cpu::is_cpu_turned_on_by_guest(vcpu_id))
            res = 0; // ON
        else
            res = 1; // OFF

        return true;
    }
    case CPU_OFF: {
        Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_off, vctx.vcpu_id, true);

        vbus.iter_devices(Model::Simple_as::flush_callback, nullptr);
        INFO("VCPU " FMTu64 " will be switched off", vctx.vcpu_id);
        res = static_cast<uint64>(SUCCESS);
        return true;
    }
    case SYSTEM_OFF:
        Lifecycle::notify_system_off(vctx);
        Vcpu::Roundup::roundup_from_vcpu(vctx.vcpu_id);
        Model::Cpu::ctrl_feature_on_all_vcpus(Model::Cpu::ctrl_feature_off, true);
        Vcpu::Roundup::resume_from_vcpu(vctx.vcpu_id);

        INFO("System was halted by the guest.");
        return true;
    case SYSTEM_RESET: {
        INFO("System reset requested by the guest.");
        Vcpu::Roundup::roundup_from_vcpu(vctx.vcpu_id);
        Model::Cpu::ctrl_feature_on_all_vcpus(Model::Cpu::ctrl_feature_off, true);
        Vcpu::Roundup::resume_from_vcpu(vctx.vcpu_id);
        vbus.reset(vctx);

        Lifecycle::notify_system_reset(vctx);

        INFO("System is now reset. Starting back...");

        // We always restart from VCPU 0 so, this is the only one that won't be off
        Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_reset, 0, true);
        Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_off, 0, false);

        return true;
    }
    }
    return false;
}
