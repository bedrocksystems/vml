/**
 * Copyright (C) 2023-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
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
        IA32_SGXLEPUBKEYHASH0 = 0x8c,
        IA32_SGXLEPUBKEYHASH1 = 0x8d,
        IA32_SGXLEPUBKEYHASH2 = 0x8e,
        IA32_SGXLEPUBKEYHASH3 = 0x8f,
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
        IA32_MTRR_PHYSBASE0 = 0x200,
        IA32_MTRR_PHYSMASK0 = 0x201,
        IA32_MTRR_PHYSBASE1 = 0x202,
        IA32_MTRR_PHYSMASK1 = 0x203,
        IA32_MTRR_PHYSBASE2 = 0x204,
        IA32_MTRR_PHYSMASK2 = 0x205,
        IA32_MTRR_PHYSBASE3 = 0x206,
        IA32_MTRR_PHYSMASK3 = 0x207,
        IA32_MTRR_PHYSBASE4 = 0x208,
        IA32_MTRR_PHYSMASK4 = 0x209,
        IA32_MTRR_PHYSBASE5 = 0x20a,
        IA32_MTRR_PHYSMASK5 = 0x20b,
        IA32_MTRR_PHYSBASE6 = 0x20c,
        IA32_MTRR_PHYSMASK6 = 0x20d,
        IA32_MTRR_PHYSBASE7 = 0x20e,
        IA32_MTRR_PHYSMASK7 = 0x20f,
        IA32_MTRR_FIX64K_00000 = 0x250,
        IA32_MTRR_FIX16K_80000 = 0x258,
        IA32_MTRR_FIX16K_A0000 = 0x259,
        IA32_MTRR_FIX4K_C0000 = 0x268,
        IA32_MTRR_FIX4K_C8000 = 0x269,
        IA32_MTRR_FIX4K_D0000 = 0x26a,
        IA32_MTRR_FIX4K_D8000 = 0x26b,
        IA32_MTRR_FIX4K_E0000 = 0x26c,
        IA32_MTRR_FIX4K_E8000 = 0x26d,
        IA32_MTRR_FIX4K_F0000 = 0x26e,
        IA32_MTRR_FIX4K_F8000 = 0x26f,
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
    bool setup_arch_msr(bool x2apic_msrs, bool mtrr, uint8 pa_width, bool sgx);
    bool setup_caps_msr(uint64 arch_caps, uint64 core_caps);

    static bool is_msr_with_addr(uint32 msrnum);

    bool setup_tsc_deadline_msr();
    bool setup_guest_state_msrs();
    bool setup_syscall_msrs();
    bool setup_sys_msrs();
    bool setup_mtrrs(bool mtrr, uint8 pa_width);

private:
    bool setup_apic_msrs(bool x2apic_msrs);

    bool setup_power_msrs();
};
