/**
 * Copyright (C) 2023 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once
#include <msr/msr_base.hpp>

namespace Msr {

    enum RegisterId : uint32 {
        IA32_TIME_STAMP_COUNTER = 0x10,
        IA32_PLATFORM_ID = 0x17,
        IA32_APICBASE = 0x1b,
        MSR_SMI_COUNT = 0x34,
        IA32_FEATURE_CONTROL = 0x3a,
        IA32_TSC_ADJUST = 0x3b,
        IA32_SPEC_CTRL = 0x48,
        IA32_PRED_CMD = 0x49,
        IA32_BIOS_SIGN_ID = 0x8b,
        IA32_FEATURE_INFO = 0xce,
        IA32_CORE_CAPABILITIES = 0xcf,
        IA32_MTRRCAPP = 0xfe,
        IA32_ARCH_CAPABILITIES = 0x10a,
        MISC_FEATURE_ENABLES = 0x140,
        IA32_SYSENTER_CS = 0x174,
        IA32_SYSENTER_ESP = 0x175,
        IA32_SYSENTER_EIP = 0x176,
        IA32_MCG_CAP = 0x179,
        IA32_MCG_STATUS = 0x17a,
        IA32_MISC_ENABLE = 0x1a0,
        IA32_PAT = 0x277,
        IA32_MTRR_DEF_TYPE = 0x2ff,
        UNCORE_CBO_CONFIG = 0x396,
        MSR_PKG_ENERGY_STATUS = 0x611,
        MSR_DRAM_ENERGY_STATUS = 0x619,
        MSR_PP0_ENERGY_STATUS = 0x639,
        MSR_PP1_ENERGY_STATUS = 0x641,
        MSR_PLATFORM_ENERGY_COUNTER = 0x64d,
        IA32_TSC_DEADLINE = 0x6e0,
        IA32_X2APIC_START = 0x800,
        IA32_X2APIC_END = 0x83f,
        IA32_XSS = 0xda0,
        UNCORE_PERF_GLOBAL_CTL = 0xe01,
        IA32_EFER = 0xc0000080,
        IA32_STAR = 0xc0000081,
        IA32_LSTAR = 0xc0000082,
        IA32_CSTAR = 0xc0000083,
        IA32_FMASK = 0xc0000084,
        IA32_FS_BASE = 0xc0000100,
        IA32_GS_BASE = 0xc0000101,
        IA32_KERNEL_GS_BASE = 0xc0000102,
        IA32_TSC_AUX = 0xc0000103,
        MSR_SEV_STATUS = 0xc0010131,
    };

    class Bus;
    class TSCRegister;
    class TscAdjust;
    class ApicBaseRegister;
    class MiscRegister;
    class SysRegister;

    typedef SysRegister GuestStateRegister;
    typedef SysRegister SysCallRegister;
}

class Msr::Bus : public Msr::BaseBus {
public:
    Bus() {}
    bool setup_arch_msr(bool x2apic_msrs);
    bool setup_caps_msr(uint64 arch_caps, uint64 core_caps);

    static bool is_msr_with_addr(uint32 msrnum);
    static bool is_x2apic_msr(uint32 msrnum);

private:
    bool setup_apic_msrs(bool x2apic_msrs);

    bool setup_guest_effective_msrs();
    bool setup_guest_state_msrs();
    bool setup_power_msrs();
    bool setup_syscall_msrs();
    bool setup_sys_msrs();
};
