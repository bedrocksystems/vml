/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <platform/bits.hpp>
#include <platform/compiler.hpp>
#include <platform/types.hpp>

namespace Msr::Info {
    enum : uint64 {
        HCR_EL2_VM = 1ull << 0,
        HCR_EL2_SWIO = 1ull << 1,
        HCR_EL2_PTW = 1ull << 2,
        HCR_EL2_FMO = 1ull << 3,
        HCR_EL2_IMO = 1ull << 4,
        HCR_EL2_AMO = 1ull << 5,
        HCR_EL2_FB = 1ull << 9,
        HCR_EL2_BSU_INNER = 1ull << 10,
        HCR_EL2_TWI = 1ull << 13,
        HCR_EL2_TWE = 1ull << 14,
        HCR_EL2_TID0 = 1ull << 15,
        HCR_EL2_TID1 = 1ull << 16,
        HCR_EL2_TID2 = 1ull << 17,
        HCR_EL2_TID3 = 1ull << 18,
        HCR_EL2_TSC = 1ull << 19,
        HCR_EL2_TIDCP = 1ull << 20,
        HCR_EL2_TACR = 1ull << 21,
        HCR_EL2_TSW = 1ull << 22,
        HCR_EL2_TVM = 1ull << 26,
        HCR_EL2_TGE = 1ull << 27,
        HCR_EL2_TDZ = 1ull << 28,
        HCR_EL2_RW = 1ull << 31,
    };

    constexpr uint64 HCR_EL2_DEFAULT_VALUE
        = HCR_EL2_VM | HCR_EL2_SWIO | HCR_EL2_PTW | HCR_EL2_FMO | HCR_EL2_IMO | HCR_EL2_AMO
          | HCR_EL2_FB | HCR_EL2_BSU_INNER | HCR_EL2_TWI | HCR_EL2_TWE | HCR_EL2_TID0 | HCR_EL2_TID1
          | HCR_EL2_TID3 | HCR_EL2_TSC | HCR_EL2_TIDCP | HCR_EL2_TACR | HCR_EL2_TSW;

    enum : uint64 {
        SCLTR_EL1_DZE = 1ull << 14,
        SCLTR_EL1_UCT = 1ull << 15,
        SCLTR_EL1_UCI = 1ull << 26,
    };

    enum : uint64 {
        CNTKCTL_EL1_EL0PCTEN = 1ull << 0,
        CNTKCTL_EL1_EL0VCTEN = 1ull << 1,
    };

    constexpr uint64 SCTLR_EL1_DEFAULT_VALUE = 0x00c50838ull;

    constexpr uint64 SPSR_MODE_MASK = 0x1full;

    enum Spsr_flags : uint64 {
        T32 = 1ull << 5,
        AARCH32 = 1ull << 4,
        AARCH64 = 0ull << 4,
        AA32_SVC = 0b0011ull,
        AA32_ABT = 0b0111ull,
        AA64_EL1 = 0b0100ull,
        AA64_SPX = 0b1ull,
        AA64_EL0 = 0b0000ull,
        AI_MASKED = 0b11ull << 7,
        F_MASKED = 0b1ull << 6,
        D_MASKED = 0b1ull << 9,
        SPSR_SINGLE_STEP = 1ull << 21,
    };

    enum Mdscr_flags {
        MDSCR_SINGLE_STEP = 0x1ull << 0,
    };

    enum {
        VMRS_SPEC_REG_FPSID = 0b0000,
        VMRS_SPEC_REG_MVFR0 = 0b0111,
        VMRS_SPEC_REG_MVFR1 = 0b0110,
        VMRS_SPEC_REG_MVFR2 = 0b0101,
    };

    class Id_aa64pfr0;
    class Id_aa64dfr0;
    class Spsr;
    class Ctr;
    class Sctlr_el1;
    class Tcr_el1;
};

class Msr::Info::Id_aa64pfr0 {
public:
    Id_aa64pfr0(uint64 val) : _value(val) {}

    enum Mode { AA64_ONLY = 0b0001ull, AA64_AA32 = 0b0010ull };
    enum Level {
        EL0_SHIFT = 0ull,
        EL1_SHIFT = 4ull,
        EL2_SHIFT = 8ull,
        EL3_SHIFT = 12ull,
    };

    Mode get_supported_mode(Level l) { return Mode((_value >> l) & MODE_MASK); }

private:
    static constexpr uint64 MODE_MASK = 0xf;
    uint64 _value;
};

class Msr::Info::Spsr {
public:
    Spsr(const uint64 val) : _val(val) {}

    static constexpr uint64 N_MASK = 1ull << 31;
    static constexpr uint64 Z_MASK = 1ull << 30;
    static constexpr uint64 C_MASK = 1ull << 29;
    static constexpr uint64 V_MASK = 1ull << 28;
    static constexpr uint64 M_MASK = 1ull << 4;
    static constexpr uint64 EL_MASK = 0xeull;
    static constexpr uint64 SP_MASK = 0x1ull;

    constexpr bool is_t32() const { return _val & T32; }
    constexpr bool is_aa32() const { return _val & M_MASK; }
    constexpr bool is_N() const { return _val & N_MASK; }
    constexpr bool is_Z() const { return _val & Z_MASK; }
    constexpr bool is_C() const { return _val & C_MASK; }
    constexpr bool is_V() const { return _val & V_MASK; }
    constexpr uint8 el() const { return _val & EL_MASK; }
    constexpr bool spx() const { return _val & SP_MASK; }

private:
    const uint64 _val;
};

class Msr::Info::Id_aa64dfr0 {
public:
    Id_aa64dfr0(uint64 val) : _value(val) {}

    uint8 debug_ver() { return _value & 0xf; }
    uint8 ctx_cmp() { return (_value >> 28) & 0xf; }
    uint8 brp() { return (_value >> 12) & 0xf; }
    uint8 wrp() { return (_value >> 20) & 0xf; }

private:
    uint64 _value;
};

class Msr::Info::Ctr {
public:
    Ctr(uint64 val) : _value(val) {}
    Ctr() { asm volatile("mrs %0, ctr_el0" : "=r"(_value) : :); }

    uint64 cache_line_size() { return 4ull << ((_value >> 16ull) & 0xfull); }

private:
    uint64 _value;
};

class Msr::Info::Sctlr_el1 {
public:
    Sctlr_el1(uint64 val) : _value(val) {}

    static constexpr uint64 CACHE_MASK = 1ull << 2;
    static constexpr uint64 MMU_MASK = 1ull << 0;

    bool mmu_enabled() const { return (_value & MMU_MASK) != 0; }
    bool cache_enabled() const { return ((_value & CACHE_MASK) != 0) && mmu_enabled(); }

private:
    uint64 _value;
};

class Msr::Info::Tcr_el1 {
public:
    Tcr_el1(uint64 val) : _value(val) {}

    enum Granule_size {
        GRANULE_16KB = 0b01,
        GRANULE_4KB = 0b10,
        GRANULE_64KB = 0b11,
    };

    Granule_size tg1() const { return Granule_size(bits_in_range(_value, 30, 31)); }
    Granule_size tg0() const { return Granule_size(bits_in_range(_value, 14, 15)); }

    bool tbi0() const { return bits_in_range(_value, 38, 38); }
    bool tbi1() const { return bits_in_range(_value, 37, 37); }

    uint8 t0sz() const { return uint8(bits_in_range(_value, 0, 5)); }
    uint8 t1sz() const { return uint8(bits_in_range(_value, 16, 21)); }

    bool eae() const { return bits_in_range(_value, 31, 31); }

    static constexpr uint8 INVALID_IPS = 0xff;

    uint8 ips() const {
        uint8 ips_bits = uint8(bits_in_range(_value, 32, 34));
        switch (ips_bits) {
        case 0b000:
            return 32;
        case 0b001:
            return 36;
        case 0b010:
            return 40;
        case 0b011:
            return 42;
        case 0b100:
            return 44;
        case 0b101:
            return 48;
        case 0b110:
            return 52;
        default:
            return INVALID_IPS;
        }
    }

private:
    uint64 _value;
};
