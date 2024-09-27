/**
 * Copyright (C) 2020-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <lifecycle.hpp>
#include <model/cpu.hpp>
#include <model/cpu_affinity.hpp>
#include <model/psci.hpp>
#include <model/vcpu_types.hpp>
#include <msr/msr_info.hpp>
#include <platform/log.hpp>
#include <platform/reg_accessor.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

static constexpr uint32 MAJOR_VERSION = 0x1u << 16;
static constexpr uint32 MINOR_VERSION = 0x1u;
static constexpr uint32 SMCCC_MAJOR_VERSION = 0x1u << 16;
static constexpr uint32 SMCCC_MINOR_VERSION = 0x1u;

enum FunctionId : uint32 {
    SMCCC_VERSION = 0x80000000u,
    SMCCC_ARCH_FEATURES = 0x80000001u,
    VERSION = 0x84000000u,
    FEATURES = 0x8400000au,
    CPU_SUSPEND_32 = 0x84000001u,
    CPU_SUSPEND_64 = 0xc4000001u,
    CPU_OFF = 0x84000002u,
    AFFINITY_INFO_32 = 0x84000004u,
    AFFINITY_INFO_64 = 0xc4000004u,
    MIGRATE_INFO_TYPE = 0x84000006u,
    CPU_ON_32 = 0x84000003u,
    CPU_ON_64 = 0xC4000003u,
    SYSTEM_OFF = 0x84000008,
    SYSTEM_RESET = 0x84000009,
    SYSTEM_SUSPEND_32 = 0x8400000eu,
    SYSTEM_SUSPEND_64 = 0xc400000eu,
};

enum PsciResult : int32 {
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

static uint64
start_cpu(RegAccessor &arch, Vbus::Bus &vbus) {
    enum Model::Cpu::StartErr err;
    Msr::Info::Spsr spsr(arch.el2_spsr());
    enum Model::Cpu::Mode mode;
    uint64 boot_addr = arch.gpr(2);
    uint64 boot_args[Model::Cpu::MAX_BOOT_ARGS] = {arch.gpr(3), 0, 0, 0};

    if (spsr.is_aa32()) {
        if ((boot_addr & 0x1ull) != 0u) {
            mode = Model::Cpu::T32;
            boot_addr = boot_addr & ~0x1ull;
        } else {
            mode = Model::Cpu::BITS_32;
        }
    } else {
        mode = Model::Cpu::BITS_64;
    }

    uint32 cpu_id = decode_cpu_id(arch.gpr(1));
    Vcpu_id vid = cpu_affinity_to_id(CpuAffinity(cpu_id));
    if (vid == INVALID_VCPU_ID) {
        WARN("Guest is trying to start VCPU#%u that is not configured by the VMM", cpu_id);
        return static_cast<uint64>(INVALID_PARAMETERS);
    }

    err = Model::Cpu::start_cpu(vid, vbus, boot_addr, boot_args, arch.tmr_cntvoff(), mode);
    return static_cast<uint64>(err);
}

static Firmware::Psci::Status
system_suspend(const VcpuCtx &vctx, uint64 &res) {
    bool all_others_off = true;

    for (uint64 id = 0; id < Model::Cpu::get_num_vcpus(); id++)
        if (id != vctx.vcpu_id)
            all_others_off = all_others_off && !Model::Cpu::is_cpu_turned_on_by_guest(id);

    if (not all_others_off) {
        res = static_cast<uint64>(DENIED);
        return Firmware::Psci::OK;
    } else {
        res = static_cast<uint64>(SUCCESS);
        return Firmware::Psci::WFI;
    }
}

Firmware::Psci::Status
Firmware::Psci::smc_call_service(const VcpuCtx &vctx, RegAccessor &arch, Vbus::Bus &vbus, uint64 const function_id, uint64 &res) {
    switch (static_cast<FunctionId>(function_id)) {
    case SMCCC_VERSION:
        res = static_cast<uint64>(SMCCC_MAJOR_VERSION | SMCCC_MINOR_VERSION);
        return OK;
    case SMCCC_ARCH_FEATURES:
        res = static_cast<uint64>(NOT_SUPPORTED); /*only version discovery is supported*/
        return OK;
    case VERSION:
        res = static_cast<uint64>(MAJOR_VERSION | MINOR_VERSION);
        return OK;
    case FEATURES: {
        uint64 feature = arch.gpr(1);
        if (feature == CPU_SUSPEND_64 || feature == CPU_SUSPEND_32)
            res = 0; /* support "Original Format" of the parameter */
        else if (feature == VERSION || feature == CPU_ON_32 || feature == CPU_ON_64 || feature == AFFINITY_INFO_64
                 || feature == AFFINITY_INFO_32 || feature == CPU_OFF || feature == SYSTEM_SUSPEND_32
                 || feature == SYSTEM_SUSPEND_64 || feature == SYSTEM_OFF || feature == SYSTEM_RESET || feature == SMCCC_VERSION)
            res = 0; // The function is present
        else
            res = static_cast<uint64>(NOT_SUPPORTED);

        return OK;
    }
    case MIGRATE_INFO_TYPE:
        res = 2; /* no migration */
        return OK;
    case CPU_ON_64:
    case CPU_ON_32:
        res = start_cpu(arch, vbus);
        return OK;
    case CPU_SUSPEND_32:
    case CPU_SUSPEND_64:
        /*
         * From the PSCI specification:
         * The powerdown request might not complete due, for example, to pending
         * interrupts. It is also possible that, because of coordination with other cores, the
         * actual state entered is shallower than the one requested. Because of this it is possible
         * for an implementation to downgrade the powerdown state request to a standby state.
         * Therefore, we can simply emulate a WFI as a first correct but sub-optimal implementation.
         */
        res = static_cast<uint64>(SUCCESS);
        return WFI;
    case AFFINITY_INFO_32:
    case AFFINITY_INFO_64: {
        Vcpu_id vcpu_id = cpu_affinity_to_id(CpuAffinity(decode_cpu_id(arch.gpr(1))));
        uint64 aff_level = arch.gpr(2);

        if (aff_level != 0) {
            res = static_cast<uint64>(INVALID_PARAMETERS); // No need to implement this with 1.0
        } else if (vcpu_id > Model::Cpu::get_num_vcpus()) {
            res = static_cast<uint64>(INVALID_PARAMETERS);
        } else {
            if (Model::Cpu::is_cpu_turned_on_by_guest(vcpu_id))
                res = 0; // ON
            else
                res = 1; // OFF
        }

        return OK;
    }
    case CPU_OFF:
        Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_off, vctx.vcpu_id, true);
        INFO("VCPU " FMTu64 " will be switched off", vctx.vcpu_id);
        res = static_cast<uint64>(SUCCESS);
        return OK;
    case SYSTEM_OFF:
        if (not Lifecycle::can_shutdown_system()) {
            /* PSCI Spec - Section 5.10: SYSTEM_OFF
             *
             * There is no standard error handling defined in the PSCI Spec.
             * It states: `If the Trusted OS requires it, provide an
             * IMPLEMENTATION-DEFINED mechanism to inform the Trusted OS
             * of the impending shutdown`
             * The SM client EC will call stop_system() anyway, so return `OK`.
             */
            INFO("A system power cycle is currently in progress, initiated from the SM");
            return OK;
        }

        Lifecycle::notify_system_off(vctx);
        Lifecycle::stop_system(vctx);

        INFO("System was halted by the guest.");
        return OK;
    case SYSTEM_RESET:
        if (not Lifecycle::can_shutdown_system()) {
            INFO("A system power cycle is currently in progress, initiated from the SM");
            return OK;
        }

        INFO("System reset requested by the guest.");
        Lifecycle::stop_system(vctx);
        vbus.reset(vctx);

        Lifecycle::notify_system_reset(vctx);

        INFO("System is now reset. Starting back...");

        Lifecycle::start_system();
        return OK;
    case SYSTEM_SUSPEND_32:
    case SYSTEM_SUSPEND_64:
        return system_suspend(vctx, res);
    }

    WARN("Unsupported PSCI call %#llx, returning NOT_SUPPORTED to the OS", function_id);
    res = static_cast<uint64>(NOT_SUPPORTED);
    return OK;
}
