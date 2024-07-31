/**
 * Copyright (C) 2023-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <model/vcpu_types.hpp>
#include <msr/msr.hpp>
#include <msr/msr_base.hpp>
#include <platform/memory.hpp>
#include <platform/new.hpp>
#include <platform/time.hpp>

class Msr::ApicBaseRegister : public Msr::Register {
private:
    static constexpr uint64 LAPIC_BASE_ADDR = 0xFEE00000;
    uint64 get_ia32_apicbase(Vcpu_id vcpu_id) const {
        static constexpr uint64 ENABLE_X2APIC_MODE = (1u << 10);
        static constexpr uint64 ENABLE_XAPIC_MODE = (1u << 11);
        static constexpr uint64 APIC_BSP = (1u << 8);

        uint64 ret = LAPIC_BASE_ADDR | ENABLE_XAPIC_MODE | (vcpu_id == 0 ? APIC_BSP : 0);
        if (_x2apic)
            ret |= ENABLE_X2APIC_MODE;

        return ret;
    }

    bool _x2apic;

public:
    explicit ApicBaseRegister(bool x2apic) : Register("IA32_APICBASE", IA32_APICBASE, false, 0x0ULL), _x2apic(x2apic) {}

    Err access(Vbus::Access access, const VcpuCtx* vctx, uint64& value) override {
        if (access == Vbus::WRITE) {
            if (value != get_ia32_apicbase(vctx->vcpu_id))
                ABORT_WITH("IA32_APICBASE is not configurable currently. Guest tried to write %#llx", value);
        } else {
            value = get_ia32_apicbase(vctx->vcpu_id);
        }

        return Err::OK;
    }
};

class Msr::MiscRegister : public Msr::Register {
public:
    MiscRegister() : Register("IA32_MISC_ENABLE", IA32_MISC_ENABLE, true, 0x1ULL) {}

    Err access(Vbus::Access access, const VcpuCtx*, uint64& value) override {
        static constexpr uint64 MASK_FAST_STRINGS = 1;
        if (access == Vbus::WRITE) {
            if ((value & MASK_FAST_STRINGS) == 0)
                WARN("Disable string operation is not supported!");

            _value = value;
        } else {
            value = _value;
        }

        return Err::OK;
    }
};

class Msr::TSCRegister : public Msr::Register {
public:
    TSCRegister() : Register("IA32_TIME_STAMP_COUNTER", IA32_TIME_STAMP_COUNTER, false, 0x0ULL) {}

    Err access(Vbus::Access access, const VcpuCtx* vctx, uint64& value) override {
        if (access == Vbus::WRITE) {
            WARN("CPU#%llu set tsc to 0x%llx", vctx->vcpu_id, value);
        } else {
            value = static_cast<uint64>(clock());
        }

        return Err::OK;
    }
};

class Msr::TscAdjust : public Msr::Register {
public:
    TscAdjust() : Register("IA32_TSC_ADJUST", IA32_TSC_ADJUST, false, 0x0ULL) {}

    Err access(Vbus::Access access, const VcpuCtx*, uint64& value) override {
        if (access == Vbus::WRITE) {
            if (value != 0)
                ABORT_WITH("TSC adjust 0x%llx", value);
        } else {
            value = 0;
        }

        return Err::OK;
    }
};

class Msr::SysRegister : public Msr::Register {
public:
    using Msr::Register::Register;

    Err access(Vbus::Access access, const VcpuCtx* vctx, uint64& value) override {
        if (access == Vbus::READ and id() == IA32_TSC_DEADLINE) {
            ABORT_WITH("read deadline back");
        } else if (access == Vbus::WRITE and id() == IA32_PAT) {
            WARN("CPU#%llu: change PAT from 0x%llx to 0x%llx", vctx->vcpu_id, _value, value);
        }

        Err status = Msr::Register::access(access, vctx, value);
        if (status == Err::OK) {
            return Err::UPDATE_REGISTER;
        }

        return status;
    }
};

bool
Msr::Bus::setup_syscall_msrs() {
    Msr::SysCallRegister* reg;

    reg = new (nothrow) Msr::SysCallRegister("IA32_STAR", IA32_STAR, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::SysCallRegister("IA32_LSTAR", IA32_LSTAR, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::SysCallRegister("IA32_CSTAR", IA32_CSTAR, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::SysCallRegister("IA32_FMASK", IA32_FMASK, true, 0x0ULL);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_guest_state_msrs() {
    Msr::GuestStateRegister* reg;

    reg = new (nothrow) Msr::GuestStateRegister("IA32_SYSENTER_CS", IA32_SYSENTER_CS, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::GuestStateRegister("IA32_SYSENTER_ESP", IA32_SYSENTER_ESP, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::GuestStateRegister("IA32_SYSENTER_EIP", IA32_SYSENTER_EIP, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::GuestStateRegister("IA32_EFER", IA32_EFER, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::GuestStateRegister("IA32_PAT", IA32_PAT, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::GuestStateRegister("IA32_FS_BASE", IA32_FS_BASE, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::GuestStateRegister("IA32_GS_BASE", IA32_GS_BASE, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::GuestStateRegister("IA32_KERNEL_GS_BASE", IA32_KERNEL_GS_BASE, true, 0x0ULL);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_apic_msrs(bool x2apic_msrs) {
    Msr::ApicBaseRegister* reg;
    reg = new (nothrow) Msr::ApicBaseRegister(x2apic_msrs);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_power_msrs() {
    Msr::Register* reg;

    reg = new (nothrow) Msr::Register("MSR_PKG_ENERGY_STATUS", MSR_PKG_ENERGY_STATUS, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("MSR_DRAM_ENERGY_STATUS", MSR_DRAM_ENERGY_STATUS, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("MSR_PP0_ENERGY_STATUS", MSR_PP0_ENERGY_STATUS, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("MSR_PP1_ENERGY_STATUS", MSR_PP1_ENERGY_STATUS, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("MSR_PLATFORM_ENERGY_COUNTER", MSR_PLATFORM_ENERGY_COUNTER, false, 0x0ULL);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_caps_msr(uint64 arch_caps, uint64 core_caps) {
    Msr::Register* reg;

    reg = new (nothrow) Msr::Register("IA32_ARCH_CAPABILITIES", IA32_ARCH_CAPABILITIES, false, arch_caps);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("IA32_CORE_CAPABILITIES", IA32_CORE_CAPABILITIES, false, core_caps);

    return register_system_reg(reg);
}

bool
Msr::Bus::setup_tsc_deadline_msr() {
    Msr::Register* reg;

    reg = new (nothrow) Msr::SysRegister("IA32_TSC_DEADLINE", IA32_TSC_DEADLINE, true, 0x0ULL);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_mtrrs(bool mtrr, uint8 pa_width) {
    static constexpr uint8 NUM_VAR_MTRR = 8;
    static constexpr uint64 MTRRCAP_VAL = 1 << 8 | NUM_VAR_MTRR;
    Msr::Register* reg;

    // Future: we could expose the WC bit if we want to support that
    reg = new (nothrow) Msr::Register("IA32_MTRRCAPP", IA32_MTRRCAPP, false, mtrr ? MTRRCAP_VAL : 0);
    if (not register_system_reg(reg))
        return false;

    static constexpr uint64 MTRR_DEF_TYPE_WMASK = 0xCFFull;
    reg = new (nothrow) Msr::Register("IA32_MTRR_DEF_TYPE", IA32_MTRR_DEF_TYPE, true, 0x0ULL, MTRR_DEF_TYPE_WMASK, true);
    if (not register_system_reg(reg))
        return false;

    if (mtrr) {
        uint64 addr_mask = mask(pa_width - PAGE_BITS, PAGE_BITS);
        static constexpr uint64 MTRR_PHYSMASK_VALID = 1ull << 11;

        for (uint8 i = 0; i < NUM_VAR_MTRR; ++i) {
            reg = new (nothrow) Msr::Register("IA32_MTRR_PHYSBASE", IA32_MTRR_PHYSBASE0 + i * 2, true, 0, addr_mask | 0xff, true);
            if (not register_system_reg(reg))
                return false;
            reg = new (nothrow)
                Msr::Register("IA32_MTRR_PHYSMASK", IA32_MTRR_PHYSMASK0 + i * 2, true, 0, addr_mask | MTRR_PHYSMASK_VALID, true);
            if (not register_system_reg(reg))
                return false;
        }

        static constexpr uint32 FIXED_MTRRS[]
            = {IA32_MTRR_FIX64K_00000, IA32_MTRR_FIX16K_80000, IA32_MTRR_FIX16K_A0000, IA32_MTRR_FIX4K_C0000,
               IA32_MTRR_FIX4K_C8000,  IA32_MTRR_FIX4K_D0000,  IA32_MTRR_FIX4K_D8000,  IA32_MTRR_FIX4K_E0000,
               IA32_MTRR_FIX4K_E8000,  IA32_MTRR_FIX4K_F0000,  IA32_MTRR_FIX4K_F8000};

        for (const auto& fixed : FIXED_MTRRS) {
            reg = new (nothrow) Msr::Register("IA32_MTRR_FIX", fixed, true, 0);
            if (not register_system_reg(reg))
                return false;
        }
    }

    return true;
}

bool
Msr::Bus::setup_arch_msr(bool x2apic_msrs, bool mtrr, uint8 pa_width, bool sgx) {
    Msr::Register* reg;

    reg = new (nothrow) Msr::Register("IA32_PLATFORM_ID", IA32_PLATFORM_ID, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("MSR_SMI_COUNT", MSR_SMI_COUNT, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow)
        Msr::Register("IA32_FEATURE_CONTROL", IA32_FEATURE_CONTROL, false, 0x1ULL | (sgx ? (bit(17) | bit(18)) : 0));
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("IA32_SPEC_CTRL", IA32_SPEC_CTRL, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("IA32_PRED_CMD", IA32_PRED_CMD, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("IA32_BIOS_SIGN_ID", IA32_BIOS_SIGN_ID, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("IA32_FEATURE_INFO", IA32_FEATURE_INFO, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    if (not setup_mtrrs(mtrr, pa_width))
        return false;

    // Ignore write
    reg = new (nothrow) Msr::Register("MISC_FEATURE_ENABLES", MISC_FEATURE_ENABLES, true, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("IA32_MCG_CAP", IA32_MCG_CAP, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("IA32_MCG_STATUS", IA32_MCG_STATUS, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("UNCORE_CBO_CONFIG", UNCORE_CBO_CONFIG, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    // Ignore write
    reg = new (nothrow) Msr::Register("UNCORE_PERF_GLOBAL_CTL", UNCORE_PERF_GLOBAL_CTL, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("MSR_SEV_STATUS", MSR_SEV_STATUS, false, 0x0ULL);
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::MiscRegister();
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::TSCRegister();
    if (not register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::TscAdjust();
    if (not register_system_reg(reg))
        return false;

    if (not setup_power_msrs()) {
        return false;
    }

    return setup_apic_msrs(x2apic_msrs);
}

bool
Msr::Bus::is_msr_with_addr(uint32 msrnum) {
    return msrnum == IA32_FS_BASE or msrnum == IA32_GS_BASE or msrnum == IA32_KERNEL_GS_BASE or msrnum == IA32_SYSENTER_CS
           or msrnum == IA32_SYSENTER_ESP or msrnum == IA32_SYSENTER_EIP;
}
