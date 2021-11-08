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
        HCR_EL2_DC = 1ull << 12,
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
        = HCR_EL2_VM | HCR_EL2_TSC | HCR_EL2_AMO | HCR_EL2_IMO | HCR_EL2_FMO;

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

    enum SpsrFlags : uint64 {
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

    enum MdscrFlags {
        MDSCR_SINGLE_STEP = 0x1ull << 0,
    };

    enum {
        VMRS_SPEC_REG_FPSID = 0b0000,
        VMRS_SPEC_REG_MVFR0 = 0b0111,
        VMRS_SPEC_REG_MVFR1 = 0b0110,
        VMRS_SPEC_REG_MVFR2 = 0b0101,
    };

    class IdAa64pfr0;
    class IdAa64dfr0;
    class Spsr;
    class Ctr;
    class SctlrEl1;
    class TcrEl1;
    class TcrEl2;
};

class Msr::Info::IdAa64pfr0 {
public:
    explicit IdAa64pfr0(uint64 val) : _value(val) {}

    enum Mode { AA64_ONLY = 0b0001ull, AA64_AA32 = 0b0010ull };
    enum Level {
        EL0_SHIFT = 0ull,
        EL1_SHIFT = 4ull,
        EL2_SHIFT = 8ull,
        EL3_SHIFT = 12ull,
    };

    Mode get_supported_mode(Level l) const { return Mode((_value >> l) & MODE_MASK); }

private:
    static constexpr uint64 MODE_MASK = 0xf;
    const uint64 _value;
};

class Msr::Info::Spsr {
public:
    explicit Spsr(const uint64 val) : _val(val) {}

    static constexpr uint64 N_MASK = 1ull << 31;
    static constexpr uint64 Z_MASK = 1ull << 30;
    static constexpr uint64 C_MASK = 1ull << 29;
    static constexpr uint64 V_MASK = 1ull << 28;
    static constexpr uint64 M_MASK = 1ull << 4;
    static constexpr uint64 EL_MASK = 0xeull;
    static constexpr uint64 SP_MASK = 0x1ull;

    constexpr bool is_t32() const { return _val & T32; }
    constexpr bool is_aa32() const { return _val & M_MASK; }
    constexpr bool is_n() const { return _val & N_MASK; }
    constexpr bool is_z() const { return _val & Z_MASK; }
    constexpr bool is_c() const { return _val & C_MASK; }
    constexpr bool is_v() const { return _val & V_MASK; }
    constexpr uint8 el() const { return _val & EL_MASK; }
    constexpr bool spx() const { return _val & SP_MASK; }

private:
    const uint64 _val;
};

class Msr::Info::IdAa64dfr0 {
public:
    explicit IdAa64dfr0(uint64 val) : _value(val) {}

    uint8 debug_ver() const { return _value & 0xf; }
    uint8 ctx_cmp() const { return (_value >> 28) & 0xf; }
    uint8 brp() const { return (_value >> 12) & 0xf; }
    uint8 wrp() const { return (_value >> 20) & 0xf; }

private:
    const uint64 _value;
};

class Msr::Info::Ctr {
public:
    explicit Ctr(uint64 val) : _value(val) {}
    Ctr() { asm volatile("mrs %0, ctr_el0" : "=r"(_value) : :); }

    bool dcache_clean_pou_for_itod() const { return !(_value & IDC_MASK); }
    bool icache_clean_pou_for_itod() const { return !(_value & DIC_MASK); }
    uint64 dcache_line_size() const { return 4ull << ((_value >> 16ull) & 0xfull); }
    uint64 icache_line_size() const { return 4ull << (_value & 0xfull); }

    enum IcachePolicy { VPIPT = 0b00, AIVIVT = 0b01, VIPT = 0b10, PIPT = 0b11 };

    IcachePolicy get_icache_policy() const { return IcachePolicy(bits_in_range(_value, 14, 15)); }

    bool can_invalidate_guest_icache() const {
        IcachePolicy icp = get_icache_policy();
        return icp == PIPT;
    }

private:
    static constexpr uint64 IDC_MASK = 1ull << 28;
    static constexpr uint64 DIC_MASK = 1ull << 29;

    uint64 _value;
};

class Msr::Info::SctlrEl1 {
public:
    explicit SctlrEl1(uint64 val) : _value(val) {}

    static constexpr uint64 CACHE_MASK = 1ull << 2;
    static constexpr uint64 MMU_MASK = 1ull << 0;

    bool mmu_enabled() const { return (_value & MMU_MASK) != 0; }
    bool cache_enabled() const { return ((_value & CACHE_MASK) != 0) && mmu_enabled(); }

private:
    const uint64 _value;
};

class Msr::Info::TcrEl1 {
public:
    explicit TcrEl1(uint64 val) : _value(val) {}

    enum GranuleSize {
        GRANULE_16KB,
        GRANULE_4KB,
        GRANULE_64KB,
        GRANULE_INVALID,
    };

    enum Tg1GranuleSize : uint64 {
        TG1_GRANULE_16KB = 0b01,
        TG1_GRANULE_4KB = 0b10,
        TG1_GRANULE_64KB = 0b11
    };

    static constexpr uint8 TG1_SHIFT = 30;
    static constexpr uint64 TG1_MASK = 0x3ull << TG1_SHIFT;

    GranuleSize tg1() const {
        uint8 bits = uint8(bits_in_range(_value, TG1_SHIFT, 31));
        switch (bits) {
        case TG1_GRANULE_16KB:
            return GRANULE_16KB;
        case TG1_GRANULE_4KB:
            return GRANULE_4KB;
        case TG1_GRANULE_64KB:
            return GRANULE_64KB;
        default:
            return GRANULE_INVALID;
        }
    }

    GranuleSize tg0() const {
        uint8 bits = uint8(bits_in_range(_value, 14, 15));
        switch (bits) {
        case 0b10:
            return GRANULE_16KB;
        case 0b00:
            return GRANULE_4KB;
        case 0b01:
            return GRANULE_64KB;
        default:
            return GRANULE_INVALID;
        }
    }

    bool tbi0() const { return bits_in_range(_value, 38, 38); }
    bool tbi1() const { return bits_in_range(_value, 37, 37); }

    static constexpr uint8 EPD1_BIT = 23;
    static constexpr uint64 EPD1_VAL = 1ull << EPD1_BIT;
    static constexpr uint8 EPD0_BIT = 7;
    static constexpr uint64 EPD0_VAL = 1ull << EPD0_BIT;

    bool epd0() const { return bits_in_range(_value, EPD0_BIT, EPD0_BIT); }
    bool epd1() const { return bits_in_range(_value, EPD1_BIT, EPD1_BIT); }

    static constexpr uint8 T0SZ_SHIFT = 0;
    static constexpr uint64 T0SZ_MASK = 0x3full << T0SZ_SHIFT;
    static constexpr uint8 T1SZ_SHIFT = 16;
    static constexpr uint64 T1SZ_MASK = 0x3full << T1SZ_SHIFT;

    uint8 t0sz() const { return uint8(bits_in_range(_value, T0SZ_SHIFT, 5)); }
    uint8 t1sz() const { return uint8(bits_in_range(_value, T1SZ_SHIFT, 21)); }

    bool eae() const { return bits_in_range(_value, 31, 31); }

    static constexpr uint8 INVALID_IPS = 0xff;

    enum IpsSize : uint64 {
        IPS_32B = 0b000,
        IPS_36B = 0b001,
        IPS_40B = 0b010,
        IPS_42B = 0b011,
        IPS_44B = 0b100,
        IPS_48B = 0b101,
        IPS_52B = 0b110
    };

    static constexpr uint8 IPS_SHIFT = 32;
    static constexpr uint64 IPS_MASK = 0x7ull << IPS_SHIFT;
    static constexpr uint8 ORGN1_SHIFT = 24;
    static constexpr uint64 ORGN1_MASK = 0x3ull << ORGN1_SHIFT;
    static constexpr uint8 IRGN1_SHIFT = 26;
    static constexpr uint64 IRGN1_MASK = 0x3ull << IRGN1_SHIFT;
    static constexpr uint8 SH1_SHIFT = 28;
    static constexpr uint64 SH1_MASK = 0x3full << SH1_SHIFT;

    enum Shareability { NON_SHAREABLE = 0b00, OUTER_SHAREABLE = 0b10, INNER_SHAREABLE = 0b11 };

    static constexpr uint8 NORMAL_MEM_WB_RWALLOC_CACHE = 0b01;

    uint8 ips() const {
        uint8 ips_bits = uint8(bits_in_range(_value, IPS_SHIFT, 34));
        switch (ips_bits) {
        case IPS_32B:
            return 32;
        case IPS_36B:
            return 36;
        case IPS_40B:
            return 40;
        case IPS_42B:
            return 42;
        case IPS_44B:
            return 44;
        case IPS_48B:
            return 48;
        case IPS_52B:
            return 52;
        default:
            return INVALID_IPS;
        }
    }

protected:
    const uint64 _value;
};

/*
 * Note that TCR_EL1 and TCR_EL2 do not share all the same fields..
 * However, some fields are the same so we can leverage that by inheriting
 * TCR_EL1 privately and only expose what makes sense.
 */
class Msr::Info::TcrEl2 : private Msr::Info::TcrEl1 {
public:
    explicit TcrEl2(uint64 val) : TcrEl1(val) {}

    uint8 t0sz() const { return TcrEl1::t0sz(); }
    GranuleSize tg0() const { return TcrEl1::tg0(); }
    uint8 start_level() const {
        uint8 level = static_cast<uint8>(bits_in_range(_value, 6, 7));

        switch (level) {
        case 0b00:
            return 2;
        case 0b01:
            return 1;
        case 0b10:
            return 0;
        case 0b11:
            return 3;
        default:
            __UNREACHED__;
        }
    }
};
