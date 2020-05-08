/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <msr/msr.hpp>
#include <types.hpp>

namespace Esr {
    class Common;
    class Msr_mrs;
    class Mcr_mrc;
    class Mcrr_mrrc;
    class Data_abort;
    class Instruction_abort;
    class Soft_step;
    class Breakpoint;

    constexpr uint8 ZERO_REG = 31;
}

class Esr::Common {
public:
    Common(uint64 const esr) : _esr(esr) {}

    bool il() const { return (_esr >> IL_SHIFT) & IL_MASK; }
    uint8 exception_class() { return (_esr >> EC_SHIFT) & EC_MASK; }

private:
    static constexpr uint64 EC_MASK = 0x3full;
    static constexpr uint8 EC_SHIFT = 26;

    static constexpr uint64 IL_MASK = 0x1ull;
    static constexpr uint8 IL_SHIFT = 25;

protected:
    uint64 const _esr;
};

class Esr::Msr_mrs : public Esr::Common {
public:
    Msr_mrs(uint64 const esr) : Common(esr) {}

    constexpr bool write() const { return !(_esr & 0x1); }
    constexpr uint8 crm() const { return (_esr >> 1) & 0xf; }
    constexpr uint8 rt() const { return (_esr >> 5) & 0x1f; }
    constexpr uint8 crn() const { return (_esr >> 10) & 0xf; }
    constexpr uint8 op1() const { return (_esr >> 14) & 0x7; }
    constexpr uint8 op2() const { return (_esr >> 17) & 0x7; }
    constexpr uint8 op0() const { return (_esr >> 20) & 0x3; }

    static constexpr uint64 ISS_MASK = 0x3fffffull;

    Msr::Id system_register() const { return Msr::Id(op0(), crn(), op1(), crm(), op2()); }
};

class Esr::Mcr_mrc : public Esr::Common {
public:
    Mcr_mrc(uint64 const esr) : Common(esr) {}

    constexpr bool write() const { return !(_esr & 0x1); }
    constexpr uint8 crm() const { return (_esr >> 1) & 0xf; }
    constexpr uint8 rt() const { return (_esr >> 5) & 0x1f; }
    constexpr uint8 crn() const { return (_esr >> 10) & 0xf; }
    constexpr uint8 opc1() const { return (_esr >> 14) & 0x7; }
    constexpr uint8 opc2() const { return (_esr >> 17) & 0x7; }

    enum Cond {
        COND_EQ = 0b0000,
        COND_NE = 0b0001,
        COND_CS = 0b0010,
        COND_CC = 0b0011,
        COND_MI = 0b0100,
        COND_PL = 0b0101,
        COND_VS = 0b0110,
        COND_VC = 0b0111,
        COND_HI = 0b1000,
        COND_LS = 0b1001,
        COND_GE = 0b1010,
        COND_LT = 0b1011,
        COND_GT = 0b1100,
        COND_LE = 0b1101,
        COND_AL = 0b1110,
    };

    constexpr enum Cond cond() const { return Cond((_esr >> 20) & 0xf); }
    constexpr uint8 cv() const { return (_esr >> 24) & 0x1; }
};

class Esr::Mcrr_mrrc : public Common {
public:
    Mcrr_mrrc(uint64 const esr) : Common(esr) {}

    constexpr bool write() const { return !(_esr & 0x1); }
    constexpr uint8 crm() const { return (_esr >> 1) & 0xf; }
    constexpr uint8 rt() const { return (_esr >> 5) & 0x1f; }
    constexpr uint8 rt2() const { return (_esr >> 10) & 0x1f; }
    constexpr uint8 opc1() const { return (_esr >> 16) & 0xf; }

    constexpr enum Esr::Mcr_mrc::Cond cond() const { return Mcr_mrc::Cond((_esr >> 20) & 0xf); }
    constexpr uint8 cv() const { return (_esr >> 24) & 0x1; }
};

class Esr::Data_abort : public Common {
    uint8 access_size() const { return (_esr >> 22) & 0x3; }

public:
    Data_abort(uint64 const esr) : Common(esr) {}

    bool isv() const { return (_esr >> 24) & 0x1; }
    uint8 reg() const { return (_esr >> 16) & 0x1f; }
    bool write() const { return (_esr >> 6) & 0x1; }
    uint8 access_size_bytes() const { return static_cast<uint8>((1 << access_size()) & 0xff); }
};

class Esr::Instruction_abort : public Esr::Common {
private:
    static constexpr uint64 SET_MASK = 0x3ull;
    static constexpr uint64 FNV_MASK = 0x1ull;
    static constexpr uint64 S1PTW_MASK = 0x1ull;
    static constexpr uint64 IFSC_MASK = 0x3full;

    static constexpr uint8 SET_SHIFT = 11;
    static constexpr uint8 FNV_SHIFT = 10;
    static constexpr uint8 S1PTW_SHIFT = 7;

public:
    Instruction_abort(uint64 const esr) : Common(esr) {}

    enum Fault_status_code {
        ADDR_SIZE_FAULT_LVL_0 = 0b000000,
        ADDR_SIZE_FAULT_LVL_1 = 0b000001,
        ADDR_SIZE_FAULT_LVL_2 = 0b000010,
        ADDR_SIZE_FAULT_LVL_3 = 0b000011,
        TRANSLATION_FAULT_LVL_0 = 0b000100,
        TRANSLATION_FAULT_LVL_1 = 0b000101,
        TRANSLATION_FAULT_LVL_2 = 0b000110,
        TRANSLATION_FAULT_LVL_3 = 0b000111,
        ACCESS_FLAG_FAULT_LVL_1 = 0b001001,
        ACCESS_FLAG_FAULT_LVL_2 = 0b001010,
        ACCESS_FLAG_FAULT_LVL_3 = 0b001011,
        PERMISSION_FAULT_LVL_1 = 0b001101,
        PERMISSION_FAULT_LVL_2 = 0b001110,
        PERMISSION_FAULT_LVL_3 = 0b001111,
    };

    enum Fault_type {
        TRANSLATION_FAULT,
        PERMISSION_FAULT,
        OTHER_FAULT,
    };

    enum Sync_err_type {
        RECOVERABLE = 0b00,
        UNCONTAINABLE = 0b01,
        RESTARTABLE_OR_CORRECTED = 0b10,
    };

    uint8 instruction_len_bytes() const { return il() ? 4 : 2; }

    Fault_status_code fault_status_code() const { return Fault_status_code(_esr & IFSC_MASK); }
    Fault_type fault_type() const {
        switch (fault_status_code()) {
        case TRANSLATION_FAULT_LVL_0:
        case TRANSLATION_FAULT_LVL_1:
        case TRANSLATION_FAULT_LVL_2:
        case TRANSLATION_FAULT_LVL_3:
            return TRANSLATION_FAULT;
        case PERMISSION_FAULT_LVL_1:
        case PERMISSION_FAULT_LVL_2:
        case PERMISSION_FAULT_LVL_3:
            return PERMISSION_FAULT;
        default:
            return OTHER_FAULT;
        }
    }

    bool stage1_page_table_walk() const { return (_esr >> S1PTW_SHIFT) & S1PTW_MASK; }
    bool far_not_valid() const { return (_esr >> FNV_SHIFT) & FNV_MASK; }
    Sync_err_type sync_err_type() const { return Sync_err_type((_esr >> SET_SHIFT) & SET_MASK); }
};

class Esr::Soft_step : public Common {
private:
    static constexpr uint64 ISV_MASK = 0x1ull << 24;
    static constexpr uint64 EX_MASK = 0x1ull << 6;

    bool isv() const { return _esr & ISV_MASK; }
    bool ex() const { return _esr & EX_MASK; }

public:
    Soft_step(uint64 const esr) : Common(esr) {}

    bool is_exclusive_load() const { return isv() && ex(); }
};

class Esr::Breakpoint : public Common {
public:
    Breakpoint(uint64 const esr) : Common(esr) {}

    bool is_thumb() const { return !il(); }
    uint16 id() const { return static_cast<uint16>(_esr); }
};
