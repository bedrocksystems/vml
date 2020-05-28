/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <model/cpu.hpp>
#include <model/psci.hpp>
#include <model/simple_as.hpp>
#include <platform/context.hpp>
#include <platform/log.hpp>
#include <platform/reg_accessor.hpp>
#include <vcpu/vcpu_roundup.hpp>

static constexpr uint32 MAJOR_VERSION = 0x1u << 16;
static constexpr uint32 MINOR_VERSION = 0x0u;

enum Function_id : uint32 {
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
Firmware::Psci::smc_call_service(Vcpu_ctx &vctx, Vbus::Bus &vbus, uint64 const function_id,
                                 uint64 &res) {
    switch (static_cast<Function_id>(function_id)) {
    case VERSION:
        res = static_cast<uint64>(MAJOR_VERSION | MINOR_VERSION);
        return true;
    case FEATURES: {
        if (function_id == CPU_SUSPEND_64 || function_id == CPU_SUSPEND_32)
            res = 0; // Function is present but not supported
        else if (function_id == VERSION || function_id == CPU_ON_32 || function_id == CPU_ON_64
                 || function_id == AFFINITY_INFO_64 || function_id == AFFINITY_INFO_32
                 || function_id == CPU_OFF || function_id == SYSTEM_OFF
                 || function_id == SYSTEM_RESET)
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
        Reg_accessor arch(*vctx.ctx, vctx.mtd_in);
        enum Model::Cpu::Start_err err;

        err = Model::Cpu::start_cpu(decode_cpu_id(arch.gpr(1)), vbus, arch.gpr(2), arch.gpr(2),
                                    arch.tmr_cntvoff());
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
        Reg_accessor arch(*vctx.ctx, vctx.mtd_in);
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
        Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_reset, vctx.vcpu_id, true);

        vbus.iter_devices(Model::Simple_as::flush_callback, nullptr);
        DEBUG("VCPU " FMTu64 " will be switched off", vctx.vcpu_id);
        res = static_cast<uint64>(SUCCESS);
        return true;
    }
    case SYSTEM_OFF:
        Vcpu::Roundup::roundup_from_vcpu(vctx.vcpu_id);
        Model::Cpu::ctrl_feature_on_all_vcpus(Model::Cpu::ctrl_feature_reset, true);
        Model::Cpu::ctrl_feature_on_all_vcpus(Model::Cpu::ctrl_feature_off, true);
        Vcpu::Roundup::resume();

        INFO("System was halted by the guest.");
        return true;
    case SYSTEM_RESET: {
        INFO("System reset requested by the guest.");
        Vcpu::Roundup::roundup_from_vcpu(vctx.vcpu_id);
        // All VCPUs are now stopped
        Model::Cpu::ctrl_feature_on_all_vcpus(Model::Cpu::ctrl_feature_reset, true);

        // We always restart from VCPU 0 so, this is the only one that won't be off
        Model::Cpu::ctrl_feature_on_all_but_vcpu(Model::Cpu::ctrl_feature_off, 0, true);
        Vcpu::Roundup::resume();

        vbus.reset();
        INFO("System is now reset. Starting back...");
        return true;
    }
    }
    return false;
}