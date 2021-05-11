/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/aa64_timer.hpp>
#include <model/cpu.hpp>
#include <model/vcpu_types.hpp>
#include <platform/log.hpp>
#include <platform/reg_accessor.hpp>
#include <platform/time.hpp>
#include <vbus/vbus.hpp>

namespace Msr {

    class Id;
    class Access;
    class RegisterBase;
    class Register;
    class Bus;
    class IdAa64pfr0;
    class IdPfr0;
    class IdPfr1;
    class Ccsidr;
    class IccSgi1rEl1;
    class CntpCtl;
    class CntpCval;
    class CntpTval;
    class CntpctEl0;
    class Set_way_flush_reg;
    class WtrappedMsr;
    class SctlrEl1;

    constexpr uint8 CCSIDR_NUM{7};

    static constexpr uint32 build_msr_id(uint8 const op0, uint8 const crn, uint8 const op1,
                                         uint8 const crm, uint8 const op2) {
        return (((uint32(crm) & 0xf) << 3) | ((uint32(crn) & 0xf) << 7)
                | ((uint32(op1) & 0x7) << 10) | ((uint32(op2) & 0x7) << 13)
                | ((uint32(op0) & 0xff) << 16));
    }

    static constexpr uint8 OP0_AARCH32_ONLY_MSR = 0xff;

    /*
     * Future, we can move all register ids declaration here so that we have an unique
     * way to identify them across the code base.
     */
    enum RegisterId : uint32 {
        CTR_A32 = build_msr_id(0b1111, 0b0, 0b0, 0b0, 0b1),
        CTR_A64 = build_msr_id(0b11, 0b0, 0b11, 0b0, 0b1),
        DCISW_A32 = build_msr_id(0b1111, 0b0111, 0b000, 0b0110, 0b010),
        DCISW_A64 = build_msr_id(0b01, 0b0111, 0b000, 0b0110, 0b010),
        DCCSW_A32 = build_msr_id(0b1111, 0b0111, 0b000, 0b1010, 0b010),
        DCCSW_A64 = build_msr_id(0b1111, 0b0111, 0b000, 0b1010, 0b010),
        DCCISW_A32 = build_msr_id(0b1111, 0b0111, 0b000, 0b1110, 0b010),
        DCCISW_A64 = build_msr_id(0b01, 0b0111, 0b000, 0b1110, 0b010),
        MVFR0 = build_msr_id(3, 0, 0, 3, 0),
        MVFR1 = build_msr_id(3, 0, 0, 3, 1),
        MVFR2 = build_msr_id(3, 0, 0, 3, 2),
        CONTEXTIDR_A32 = build_msr_id(0b1111, 0xd, 0, 0, 1),
        CONTEXTIDR_EL1 = build_msr_id(3, 0xd, 0, 3, 1),
        SCTLR_EL1 = Msr::build_msr_id(3, 1, 0, 0, 0),
        TTBR0_EL1 = Msr::build_msr_id(3, 2, 0, 0, 0),
        TTBR0_A32 = Msr::build_msr_id(0b1111, 0, 0, 2, 0),
        TTBR1_EL1 = Msr::build_msr_id(3, 2, 0, 0, 1),
        TTBR1_A32 = Msr::build_msr_id(0b1111, 0, 1, 2, 0),
        TCR_EL1 = Msr::build_msr_id(3, 2, 0, 0, 2),
        AFSR0_EL1 = Msr::build_msr_id(3, 5, 0, 1, 0),
        AFSR1_EL1 = Msr::build_msr_id(3, 5, 0, 1, 1),
        ESR_EL1 = Msr::build_msr_id(3, 5, 0, 2, 0),
        FAR_EL1 = Msr::build_msr_id(3, 6, 0, 0, 0),
        MAIR_EL1 = Msr::build_msr_id(3, 0xa, 0, 2, 0),
        AMAIR_EL1 = Msr::build_msr_id(3, 0xa, 0, 3, 0),
        CNTPCT_A32 = Msr::build_msr_id(0b1111, 0, 0, 0xe, 0),
        CNTP_CVAL_A32 = Msr::build_msr_id(0b1111, 0, 2, 0xe, 0),
        CNTP_CTL_A32 = Msr::build_msr_id(0b1111, 0xe, 0, 2, 1),
        CNTP_TVAL_A32 = Msr::build_msr_id(0b1111, 0xe, 0, 2, 0),
        CNTPCT_EL0 = Msr::build_msr_id(3, 0xe, 3, 0, 1),
        CNTP_CVAL_EL0 = Msr::build_msr_id(3, 0xe, 3, 2, 2),
        CNTP_CTL_EL0 = Msr::build_msr_id(3, 0xe, 3, 2, 1),
        CNTP_TVAL_EL0 = Msr::build_msr_id(3, 0xe, 3, 2, 0),
        ID_AA64MMFR0_EL1 = Msr::build_msr_id(3, 0, 0, 7, 0),
        ID_AA64PFR0_EL1 = Msr::build_msr_id(3, 0, 0, 4, 0),
        CCSIDR_EL1 = Msr::build_msr_id(3, 0, 1, 0, 0),
        ID_PFR0_EL1 = Msr::build_msr_id(3, 0, 0, 1, 0),
        ID_PFR1_EL1 = Msr::build_msr_id(3, 0, 0, 1, 1),
        ID_AA64DFR0_EL1 = build_msr_id(3, 0, 0, 5, 0),
        ID_AA64DFR1_EL1 = build_msr_id(3, 0, 0, 5, 1),
        MDSCR_EL1 = build_msr_id(2, 0, 0, 2, 2),

        DBGBVR0_EL1 = build_msr_id(2, 0, 0, 0, 4),
        DBGBVR1_EL1 = build_msr_id(2, 0, 0, 1, 4),
        DBGBVR2_EL1 = build_msr_id(2, 0, 0, 2, 4),
        DBGBVR3_EL1 = build_msr_id(2, 0, 0, 3, 4),
        DBGBVR4_EL1 = build_msr_id(2, 0, 0, 4, 4),
        DBGBVR5_EL1 = build_msr_id(2, 0, 0, 5, 4),
        DBGBVR6_EL1 = build_msr_id(2, 0, 0, 6, 4),
        DBGBVR7_EL1 = build_msr_id(2, 0, 0, 7, 4),
        DBGBVR8_EL1 = build_msr_id(2, 0, 0, 8, 4),
        DBGBVR9_EL1 = build_msr_id(2, 0, 0, 9, 4),
        DBGBVR10_EL1 = build_msr_id(2, 0, 0, 10, 4),
        DBGBVR11_EL1 = build_msr_id(2, 0, 0, 11, 4),
        DBGBVR12_EL1 = build_msr_id(2, 0, 0, 12, 4),
        DBGBVR13_EL1 = build_msr_id(2, 0, 0, 13, 4),
        DBGBVR14_EL1 = build_msr_id(2, 0, 0, 14, 4),
        DBGBVR15_EL1 = build_msr_id(2, 0, 0, 15, 4),

        DBGBCR0_EL1 = build_msr_id(2, 0, 0, 0, 5),
        DBGBCR1_EL1 = build_msr_id(2, 0, 0, 1, 5),
        DBGBCR2_EL1 = build_msr_id(2, 0, 0, 2, 5),
        DBGBCR3_EL1 = build_msr_id(2, 0, 0, 3, 5),
        DBGBCR4_EL1 = build_msr_id(2, 0, 0, 4, 5),
        DBGBCR5_EL1 = build_msr_id(2, 0, 0, 5, 5),
        DBGBCR6_EL1 = build_msr_id(2, 0, 0, 6, 5),
        DBGBCR7_EL1 = build_msr_id(2, 0, 0, 7, 5),
        DBGBCR8_EL1 = build_msr_id(2, 0, 0, 8, 5),
        DBGBCR9_EL1 = build_msr_id(2, 0, 0, 9, 5),
        DBGBCR10_EL1 = build_msr_id(2, 0, 0, 10, 5),
        DBGBCR11_EL1 = build_msr_id(2, 0, 0, 11, 5),
        DBGBCR12_EL1 = build_msr_id(2, 0, 0, 12, 5),
        DBGBCR13_EL1 = build_msr_id(2, 0, 0, 13, 5),
        DBGBCR14_EL1 = build_msr_id(2, 0, 0, 14, 5),
        DBGBCR15_EL1 = build_msr_id(2, 0, 0, 15, 5),

        DBGWVR0_EL1 = build_msr_id(2, 0, 0, 0, 6),
        DBGWVR1_EL1 = build_msr_id(2, 0, 0, 1, 6),
        DBGWVR2_EL1 = build_msr_id(2, 0, 0, 2, 6),
        DBGWVR3_EL1 = build_msr_id(2, 0, 0, 3, 6),
        DBGWVR4_EL1 = build_msr_id(2, 0, 0, 4, 6),
        DBGWVR5_EL1 = build_msr_id(2, 0, 0, 5, 6),
        DBGWVR6_EL1 = build_msr_id(2, 0, 0, 6, 6),
        DBGWVR7_EL1 = build_msr_id(2, 0, 0, 7, 6),
        DBGWVR8_EL1 = build_msr_id(2, 0, 0, 8, 6),
        DBGWVR9_EL1 = build_msr_id(2, 0, 0, 9, 6),
        DBGWVR10_EL1 = build_msr_id(2, 0, 0, 10, 6),
        DBGWVR11_EL1 = build_msr_id(2, 0, 0, 11, 6),
        DBGWVR12_EL1 = build_msr_id(2, 0, 0, 12, 6),
        DBGWVR13_EL1 = build_msr_id(2, 0, 0, 13, 6),
        DBGWVR14_EL1 = build_msr_id(2, 0, 0, 14, 6),
        DBGWVR15_EL1 = build_msr_id(2, 0, 0, 15, 6),

        DBGWCR0_EL1 = build_msr_id(2, 0, 0, 0, 7),
        DBGWCR1_EL1 = build_msr_id(2, 0, 0, 1, 7),
        DBGWCR2_EL1 = build_msr_id(2, 0, 0, 2, 7),
        DBGWCR3_EL1 = build_msr_id(2, 0, 0, 3, 7),
        DBGWCR4_EL1 = build_msr_id(2, 0, 0, 4, 7),
        DBGWCR5_EL1 = build_msr_id(2, 0, 0, 5, 7),
        DBGWCR6_EL1 = build_msr_id(2, 0, 0, 6, 7),
        DBGWCR7_EL1 = build_msr_id(2, 0, 0, 7, 7),
        DBGWCR8_EL1 = build_msr_id(2, 0, 0, 8, 7),
        DBGWCR9_EL1 = build_msr_id(2, 0, 0, 9, 7),
        DBGWCR10_EL1 = build_msr_id(2, 0, 0, 10, 7),
        DBGWCR11_EL1 = build_msr_id(2, 0, 0, 11, 7),
        DBGWCR12_EL1 = build_msr_id(2, 0, 0, 12, 7),
        DBGWCR13_EL1 = build_msr_id(2, 0, 0, 13, 7),
        DBGWCR14_EL1 = build_msr_id(2, 0, 0, 14, 7),
        DBGWCR15_EL1 = build_msr_id(2, 0, 0, 15, 7),

        MDRAR_EL1 = build_msr_id(2, 1, 0, 0, 0),
        ID_DFR0_EL1 = build_msr_id(3, 0, 0, 1, 2),
        ID_DFR1_EL1 = build_msr_id(3, 0, 0, 3, 5),
        DBGDIDR = build_msr_id(2, 0, 0, 0, 0),
        ACTLR_EL1 = build_msr_id(3, 1, 0, 0, 1),
        DBGAUTHSTATUS_EL1 = build_msr_id(2, 7, 0, 0xe, 6),
        ID_AA64PFR1_EL1 = build_msr_id(3, 0, 0, 4, 1),
        ID_AA64ZFR0_EL1 = build_msr_id(3, 0, 0, 4, 4),
        ID_AA64ISAR0_EL1 = build_msr_id(3, 0, 0, 6, 0),
        ID_AA64ISAR1_EL1 = build_msr_id(3, 0, 0, 6, 1),
        ID_AA64AFR0_EL1 = build_msr_id(3, 0, 0, 5, 4),
        ID_AA64AFR1_EL1 = build_msr_id(3, 0, 0, 5, 5),
        ID_PFR2_EL1 = build_msr_id(3, 0, 0, 3, 4),
        ID_ISAR0_EL1 = build_msr_id(3, 0, 0, 2, 0),
        ID_ISAR1_EL1 = build_msr_id(3, 0, 0, 2, 1),
        ID_ISAR2_EL1 = build_msr_id(3, 0, 0, 2, 2),
        ID_ISAR3_EL1 = build_msr_id(3, 0, 0, 2, 3),
        ID_ISAR4_EL1 = build_msr_id(3, 0, 0, 2, 4),
        ID_ISAR5_EL1 = build_msr_id(3, 0, 0, 2, 5),
        ID_ISAR6_EL1 = build_msr_id(3, 0, 0, 2, 7),
        ID_AA64MMFR1_EL1 = build_msr_id(3, 0, 0, 7, 1),
        ID_AA64MMFR2_EL1 = build_msr_id(3, 0, 0, 7, 2),
        ID_MMFR0_EL1 = build_msr_id(3, 0, 0, 1, 4),
        ID_MMFR1_EL1 = build_msr_id(3, 0, 0, 1, 5),
        ID_MMFR2_EL1 = build_msr_id(3, 0, 0, 1, 6),
        ID_MMFR3_EL1 = build_msr_id(3, 0, 0, 1, 7),
        ID_MMFR4_EL1 = build_msr_id(3, 0, 0, 2, 6),
        ID_MMFR5_EL1 = build_msr_id(3, 0, 0, 3, 6),
        CLIDR_EL1 = build_msr_id(3, 0, 1, 0, 1),
        CSSELR_EL1 = build_msr_id(3, 0, 2, 0, 0),
        AIDR_EL1 = build_msr_id(3, 0, 1, 0, 7),
        REVIDR_EL1 = build_msr_id(3, 0, 0, 0, 6),
        PMUSEREN_EL0 = build_msr_id(3, 9, 3, 14, 0),
        OSDLR_EL1 = build_msr_id(2, 1, 0, 3, 4),
        OSLAR_EL1 = build_msr_id(2, 1, 0, 0, 4),
        ID_AFR0_EL1 = build_msr_id(3, 0, 0, 1, 3),

        ICC_PMR_EL1 = build_msr_id(3, 4, 0, 6, 0),
        ICC_AP1R0_EL1 = build_msr_id(3, 12, 0, 9, 0),
        ICC_AP1R1_EL1 = build_msr_id(3, 12, 0, 9, 1),
        ICC_AP1R2_EL1 = build_msr_id(3, 12, 0, 9, 2),
        ICC_AP1R3_EL1 = build_msr_id(3, 12, 0, 9, 3),
        ICC_DIR_EL1 = build_msr_id(3, 12, 0, 11, 1),
        ICC_RPR_EL1 = build_msr_id(3, 12, 0, 11, 3),
        ICC_SGI1R_EL1 = build_msr_id(3, 0xc, 0x0, 0xb, 5),
        ICC_IAR1_EL1 = build_msr_id(3, 12, 0, 12, 0),
        ICC_EOIR1_EL1 = build_msr_id(3, 12, 0, 12, 1),
        ICC_HPPIR1_EL1 = build_msr_id(3, 12, 0, 12, 2),
        ICC_BPR1_EL1 = build_msr_id(3, 12, 0, 12, 3),
        ICC_CTLR_EL1 = build_msr_id(3, 12, 0, 12, 4),
        ICC_SRE_EL1 = build_msr_id(3, 12, 0, 12, 5),
        ICC_IGRPEN1_EL1 = build_msr_id(3, 12, 0, 12, 7),
        ICC_SRE_EL2 = build_msr_id(3, 12, 4, 9, 5),

        CNTHP_TVAL_EL2 = build_msr_id(3, 14, 4, 2, 0),
        CNTHP_CTL_EL2 = build_msr_id(3, 14, 4, 2, 1),
        CNTHP_CVAL_EL2 = build_msr_id(3, 14, 4, 2, 2),

        /*
         * Below, we define a namespace for registers that do no exist in AA64.
         * We know we are not overlapping with AA64 or AA32 there because op0 (or coproc)
         * is only 4 bits on AA32 and 2 bits in AA64.
         */
        FPSID = build_msr_id(OP0_AARCH32_ONLY_MSR, 0, 0, 3, 0),
        DACR_A32 = build_msr_id(0b1111, 0b0011, 0b000, 0b0000, 0b000),
        DACR = build_msr_id(OP0_AARCH32_ONLY_MSR, 0b0011, 0b000, 0b0000, 0b000),
        MAIR1_A32 = Msr::build_msr_id(3, 0xa, 0, 2, 1),
        IFSR_A32 = build_msr_id(0b1111, 0b0101, 0b000, 0b0000, 0b001),
        IFSR = build_msr_id(OP0_AARCH32_ONLY_MSR, 0b0101, 0b000, 0b0000, 0b001),
        JIDR_A32 = build_msr_id(0b1110, 0b0000, 0b111, 0b0000, 0b000),
        JIDR = build_msr_id(OP0_AARCH32_ONLY_MSR, 0b0000, 0b111, 0b0000, 0b000),
        FCSEIDR_A32 = build_msr_id(0b1111, 0b1101, 0b000, 0b0000, 0b000),
        FCSEIDR = build_msr_id(OP0_AARCH32_ONLY_MSR, 0b1101, 0b000, 0b0000, 0b000),
        TCMTR_A32 = build_msr_id(0b1111, 0b0000, 0b000, 0b0000, 0b010),
        TCMTR = build_msr_id(OP0_AARCH32_ONLY_MSR, 0b0000, 0b000, 0b0000, 0b010),
        TLBTR_A32 = build_msr_id(0b1111, 0b0000, 0b000, 0b0000, 0b011),
        TLBTR = build_msr_id(OP0_AARCH32_ONLY_MSR, 0b0000, 0b000, 0b0000, 0b011),
        INVALID_ID = build_msr_id(0xff, 0xff, 0xff, 0xff, 0xff),
    };

    static constexpr uint32 DBGBVR_EL1[]
        = {DBGBVR0_EL1,  DBGBVR1_EL1,  DBGBVR2_EL1,  DBGBVR3_EL1, DBGBVR4_EL1,  DBGBVR5_EL1,
           DBGBVR6_EL1,  DBGBVR7_EL1,  DBGBVR8_EL1,  DBGBVR9_EL1, DBGBVR10_EL1, DBGBVR11_EL1,
           DBGBVR12_EL1, DBGBVR13_EL1, DBGBVR14_EL1, DBGBVR15_EL1};

    static constexpr uint32 DBGBCR_EL1[]
        = {DBGBCR0_EL1,  DBGBCR1_EL1,  DBGBCR2_EL1,  DBGBCR3_EL1, DBGBCR4_EL1,  DBGBCR5_EL1,
           DBGBCR6_EL1,  DBGBCR7_EL1,  DBGBCR8_EL1,  DBGBCR9_EL1, DBGBCR10_EL1, DBGBCR11_EL1,
           DBGBCR12_EL1, DBGBCR13_EL1, DBGBCR14_EL1, DBGBCR15_EL1};

    static constexpr uint32 DBGWVR_EL1[]
        = {DBGWVR0_EL1,  DBGWVR1_EL1,  DBGWVR2_EL1,  DBGWVR3_EL1, DBGWVR4_EL1,  DBGWVR5_EL1,
           DBGWVR6_EL1,  DBGWVR7_EL1,  DBGWVR8_EL1,  DBGWVR9_EL1, DBGWVR10_EL1, DBGWVR11_EL1,
           DBGWVR12_EL1, DBGWVR13_EL1, DBGWVR14_EL1, DBGWVR15_EL1};

    static constexpr uint32 DBGWCR_EL1[]
        = {DBGWCR0_EL1,  DBGWCR1_EL1,  DBGWCR2_EL1,  DBGWCR3_EL1, DBGWCR4_EL1,  DBGWCR5_EL1,
           DBGWCR6_EL1,  DBGWCR7_EL1,  DBGWCR8_EL1,  DBGWCR9_EL1, DBGWCR10_EL1, DBGWCR11_EL1,
           DBGWCR12_EL1, DBGWCR13_EL1, DBGWCR14_EL1, DBGWCR15_EL1};

    void flush_on_cache_toggle(const VcpuCtx* vcpu, Vbus::Bus& vbus, uint64 new_value);
}

namespace Model {
    class GicD;
}

static constexpr uint32
build_msr_id(uint8 const op0, uint8 const crn, uint8 const op1, uint8 const crm, uint8 const op2) {
    return (((uint32(crm) & 0xf) << 1) | ((uint32(crn) & 0xf) << 10) | ((uint32(op1) & 0x7) << 14)
            | ((uint32(op2) & 0x7) << 17) | ((uint32(op0) & 0x3) << 20))
           << 2;
}

class Msr::Id {
private:
    uint32 _id;

public:
    /* align id 8 byte for vbus usage */
    Id(uint8 const op0, uint8 const crn, uint8 const op1, uint8 const crm, uint8 const op2)
        : _id(build_msr_id(op0, crn, op1, crm, op2)) {}

    /*
     * We are bending the rules a bit for this class. It is useful to have a conversion from uint32
     * to an ID without being explicit because most IDs are stored in an enum as uint32. We still
     * want to use those uint32 as Ids transparently whenever possible.
     */
    Id(uint32 id) : _id(id) {} // NOLINT

    uint32 id() const { return _id; }
};

class Msr::Access {
public:
    static constexpr uint8 INVALID_REG_ACCESS = 0xff;

    Access(uint8 const op0, uint8 const crn, uint8 const op1, uint8 const crm, uint8 const op2,
           uint8 const gpr_target, bool write)
        : _write(write), _target(gpr_target), _second_target(INVALID_REG_ACCESS),
          _id(build_msr_id(op0, crn, op1, crm, op2)) {}
    Access(uint32 id, uint8 const gpr_target, bool write)
        : _write(write), _target(gpr_target), _second_target(INVALID_REG_ACCESS), _id(id) {}

    bool write() const { return _write; }
    uint8 target_reg() const { return _target; }
    uint32 id() const { return _id.id(); }

    // only useful for 32-bit when writing to 64-bit system registers
    bool double_target_reg() const { return _second_target != INVALID_REG_ACCESS; }
    void set_second_target_reg(uint8 t) { _second_target = t; }
    uint8 second_target_reg() const {
        ASSERT(double_target_reg());
        return _second_target;
    }

private:
    bool _write;
    uint8 _target;
    uint8 _second_target;
    Msr::Id _id;
};

class Msr::RegisterBase : public Vbus::Device {
public:
    RegisterBase(const char* name, Id reg_id) : Vbus::Device::Device(name), _reg_id(reg_id) {}

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, Vbus::Space, mword off,
                             uint8 bytes, uint64& res)
        = 0;

    uint32 id() const { return _reg_id.id(); };

private:
    Id _reg_id;
};

class Msr::Register : public RegisterBase {
protected:
    uint64 _value;
    uint64 _reset_value;

private:
    uint64 const _write_mask;
    bool const _writable;

public:
    Register(const char* name, Id const reg_id, bool const writable, uint64 const reset_value,
             uint64 const mask = ~0ULL)
        : RegisterBase(name, reg_id), _value(reset_value), _reset_value(reset_value),
          _write_mask(mask), _writable(writable) {}

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx*, Vbus::Space, mword, uint8,
                             uint64& value) override {
        if (access == Vbus::WRITE && !_writable)
            return Vbus::Err::ACCESS_ERR;

        if (access == Vbus::WRITE) {
            _value |= value & _write_mask;                    // Set the bits at 1
            _value &= ~(_write_mask & (value ^ _write_mask)); // Set the bits at 0
        } else {
            value = _value;
        }

        return Vbus::Err::OK;
    }

    virtual void reset(const VcpuCtx*) override { _value = _reset_value; }
};

class Msr::Set_way_flush_reg : public Msr::Register {
public:
    Set_way_flush_reg(const char* name, Id const reg_id, Vbus::Bus& vbus)
        : Register(name, reg_id, true, 0x0, 0x00000000fffffffeull), _vbus(&vbus) {}
    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx* vctx, Vbus::Space sp, mword off,
                             uint8 bytes, uint64& value) override {
        Vbus::Err ret = Register::access(access, vctx, sp, off, bytes, value);

        if (access == Vbus::Access::WRITE) {
            flush(vctx, (_value >> 1) & 0x7, static_cast<uint32>(_value >> 4));
        }

        return ret;
    }

protected:
    void flush(const VcpuCtx*, uint8, uint32) const;

    Vbus::Bus* _vbus;
};

class Msr::IdAa64pfr0 : public Register {
private:
    uint64 _reset_value(uint64 value) const {
        value &= ~(0xfull << 28); /* ras - not implemented */
        value &= ~(0xfull << 32); /* sve - not implemented */
        value &= ~(0xfull << 40); /* MPAM - not implemented */
        value &= ~(0xfull << 44); /* AMU - not implemented */
        return value;
    }

public:
    explicit IdAa64pfr0(uint64 value)
        : Register("ID_AA64PFR0_EL1", ID_AA64PFR0_EL1, false, _reset_value(value)) {}
};

class Msr::IdPfr0 : public Register {
private:
    uint64 _reset_value(uint32 value) const {
        /* Take the value of the HW for state 0 to 3 (bits[15:0]), the rest is not implemented */
        return value & 0xffffull;
    }

public:
    explicit IdPfr0(uint32 value)
        : Register("ID_PFR0_EL1", ID_PFR0_EL1, false, _reset_value(value)) {}
};

class Msr::IdPfr1 : public Register {
private:
    uint64 _reset_value(uint32 value) const {
        uint64 ret = value;
        /* Disable the features that require aarch32 el1 to be implemented */
        ret &= ~(0xfull);       /* Disable ProgMod */
        ret &= ~(0xfull << 4);  /* Disable security */
        ret &= ~(0xfull << 12); /* Disable Virt */
        return ret;
    }

public:
    explicit IdPfr1(uint32 value)
        : Register("ID_PFR1_EL1", ID_PFR1_EL1, false, _reset_value(value)) {}
};

class Msr::Ccsidr : public RegisterBase {
private:
    Register& _csselr;
    uint64 const _clidr_el1;
    uint64 _ccsidr_data_el1[CCSIDR_NUM]{};
    uint64 _ccsidr_inst_el1[CCSIDR_NUM]{};

    enum CacheEntry {
        NO_CACHE = 0,
        INSTRUCTION_CACHE_ONLY = 1,
        DATA_CACHE_ONLY = 2,
        SEPARATE_CACHE = 3,
        UNIFIED_CACHE = 4,
        INVALID = 0xffffffff
    };

public:
    Ccsidr(Register& cs, uint64 const clidr, uint64 const* ccsidr)
        : RegisterBase("CCSIDR_EL1", CCSIDR_EL1), _csselr(cs), _clidr_el1(clidr) {
        for (unsigned level = 0; level < CCSIDR_NUM; level++) {
            _ccsidr_data_el1[level] = ccsidr[level * 2];
            _ccsidr_inst_el1[level] = ccsidr[level * 2 + 1];
        }
    }

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, Vbus::Space sp, mword,
                             uint8, uint64& value) override {
        if (access == Vbus::WRITE)
            return Vbus::Err::ACCESS_ERR;

        uint64 el = 0;
        if (Vbus::Err::OK
            != _csselr.access(access, vcpu_ctx, sp, 0 /* offset */, 4 /* bytes */, el))
            return Vbus::Err::ACCESS_ERR;

        bool const instr = el & 0x1;
        unsigned const level = (el >> 1) & 0x7;

        if (level > 6)
            return Vbus::Err::ACCESS_ERR;

        uint8 const ce = (_clidr_el1 >> (level * 3)) & 0b111;

        if (ce == NO_CACHE || (ce == DATA_CACHE_ONLY && instr)) {
            value = INVALID;
            return Vbus::Err::OK;
        }
        if (ce == INSTRUCTION_CACHE_ONLY || (ce == SEPARATE_CACHE && instr)) {
            value = _ccsidr_inst_el1[level];
            return Vbus::Err::OK;
        }

        value = _ccsidr_data_el1[level];
        return Vbus::Err::OK;
    }

    virtual void reset(const VcpuCtx*) override {}
};

class Msr::IccSgi1rEl1 : public RegisterBase {
private:
    Model::GicD* _gic;

public:
    explicit IccSgi1rEl1(Model::GicD& gic)
        : RegisterBase("ICC_SGI1R_EL1", ICC_SGI1R_EL1), _gic(&gic) {}

    virtual Vbus::Err access(Vbus::Access, const VcpuCtx*, Vbus::Space, mword, uint8,
                             uint64&) override;

    virtual void reset(const VcpuCtx*) override {}
};

class Msr::CntpCtl : public Register {
private:
    Model::AA64Timer* _ptimer;

public:
    CntpCtl(const char* name, Msr::RegisterId id, Model::AA64Timer& t)
        : Register(name, id, true, 0, 0b11), _ptimer(&t) {}

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, Vbus::Space sp,
                             mword addr, uint8 size, uint64& value) {
        _value = _ptimer->get_ctl();

        Vbus::Err err = Register::access(access, vcpu_ctx, sp, addr, size, value);
        if (err == Vbus::OK && access == Vbus::WRITE) {
            _ptimer->set_ctl(static_cast<uint8>(_value));
        }

        return err;
    }
};

class Msr::CntpCval : public Register {
private:
    Model::AA64Timer* _ptimer;

public:
    CntpCval(const char* name, Msr::RegisterId id, Model::AA64Timer& t)
        : Register(name, id, true, 0), _ptimer(&t) {}

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, Vbus::Space sp,
                             mword addr, uint8 size, uint64& value) {
        _value = _ptimer->get_cval();

        Vbus::Err err = Register::access(access, vcpu_ctx, sp, addr, size, value);
        if (err == Vbus::OK && access == Vbus::WRITE) {
            _ptimer->set_cval(_value);
        }

        return err;
    }
};

class Msr::CntpctEl0 : public RegisterBase {
public:
    explicit CntpctEl0() : RegisterBase("CNTPCT_EL0", CNTPCT_EL0) {}

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx* vctx, Vbus::Space, mword, uint8,
                             uint64& value) override {
        if (access != Vbus::READ)
            return Vbus::Err::ACCESS_ERR;

        value = static_cast<uint64>(clock()) - vctx->regs->tmr_cntvoff();
        return Vbus::Err::OK;
    }

    virtual void reset(const VcpuCtx*) override {}
};

class Msr::CntpTval : public Register {
private:
    Model::AA64Timer* _ptimer;
    static constexpr uint64 CNTP_TVAL_MASK = 0xffffffffull;

public:
    CntpTval(const char* name, Msr::RegisterId id, Model::AA64Timer& t)
        : Register(name, id, true, 0, CNTP_TVAL_MASK), _ptimer(&t) {}

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx* vctx, Vbus::Space, mword, uint8,
                             uint64& value) {
        if (access == Vbus::READ) {
            uint64 cval = _ptimer->get_cval(),
                   curr = static_cast<uint64>(clock()) - vctx->regs->tmr_cntvoff();
            value = (cval - curr) & CNTP_TVAL_MASK;
            return Vbus::Err::OK;
        } else if (access == Vbus::WRITE) {
            int32 v = static_cast<int32>(value);
            _ptimer->set_cval(static_cast<uint64>(clock()) + static_cast<uint64>(v));
            return Vbus::Err::OK;
        } else {
            return Vbus::Err::ACCESS_ERR;
        }
    }
};

class Msr::WtrappedMsr : public Msr::RegisterBase {
public:
    using Msr::RegisterBase::RegisterBase;

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx*, Vbus::Space, mword, uint8,
                             uint64&) override {
        ASSERT(access == Vbus::Access::WRITE); // We only trap writes at the moment
        return Vbus::Err::UPDATE_REGISTER;     // Tell the VCPU to update the relevant physical
                                               // register
    }
    virtual void reset(const VcpuCtx*) override {}
};

class Msr::SctlrEl1 : public Msr::RegisterBase {
public:
    SctlrEl1(const char* name, Msr::Id reg_id, Vbus::Bus& vbus)
        : Msr::RegisterBase(name, reg_id), _vbus(&vbus) {}

    virtual Vbus::Err access(Vbus::Access access, const VcpuCtx* vcpu, Vbus::Space, mword, uint8,
                             uint64& res) override;
    virtual void reset(const VcpuCtx*) override {}

private:
    Vbus::Bus* _vbus;
};

/*
 * This is the bus that will handle all reads and writes
 * to system registers.
 */
class Msr::Bus : public Vbus::Bus {
public:
    Bus() : Vbus::Bus(Vbus::Space::SYSTEM_REGISTER) {}

    struct AA64PlatformInfo {
        uint64 id_aa64pfr0_el1;
        uint64 id_aa64pfr1_el1;
        uint64 id_aa64dfr0_el1;
        uint64 id_aa64dfr1_el1;
        uint64 id_aa64isar0_el1;
        uint64 id_aa64isar1_el1;
        uint64 id_aa64isar2_el1;
        uint64 id_aa64mmfr0_el1;
        uint64 id_aa64mmfr1_el1;
        uint64 id_aa64mmfr2_el1;
        uint64 id_aa64zfr0_el1;
        uint64 midr_el1;
    };

    struct AA32PlatformInfo {
        uint32 id_pfr0_el1;
        uint32 id_pfr1_el1;
        uint32 id_pfr2_el1;
        uint32 id_dfr0_el1;
        uint32 id_dfr1_el1;
        uint32 id_isar0_el1;
        uint32 id_isar1_el1;
        uint32 id_isar2_el1;
        uint32 id_isar3_el1;
        uint32 id_isar4_el1;
        uint32 id_isar5_el1;
        uint32 id_isar6_el1;
        uint32 id_mmfr0_el1;
        uint32 id_mmfr1_el1;
        uint32 id_mmfr2_el1;
        uint32 id_mmfr3_el1;
        uint32 id_mmfr4_el1;
        uint32 id_mmfr5_el1;
        uint32 mvfr0_el1;
        uint32 mvfr1_el1;
        uint32 mvfr2_el1;
    };

    struct PlatformInfo {
        AA64PlatformInfo aa64;
        AA32PlatformInfo aa32;
    };

private:
    bool setup_aarch64_features(uint64 id_aa64pfr0_el1, uint64 id_aa64pfr1_el1,
                                uint64 id_aa64isar0_el1, uint64 id_aa64isar1_el1,
                                uint64 id_aa64zfr0_el1);
    bool setup_aarch64_setway_flushes(Vbus::Bus& vbus);
    bool setup_aarch64_memory_model(uint64 id_aa64mmfr0_el1, uint64 id_aa64mmfr1_el1,
                                    uint64 id_aa64mmfr2_el1);
    bool setup_aarch64_debug(uint64 id_aa64dfr0_el1, uint64 id_aa64dfr1_el1);
    bool setup_aarch64_auxiliary();

    bool setup_aarch32_features(const AA32PlatformInfo& aa32);
    bool setup_aarch32_memory_model(uint32 id_mmfr0_el1, uint32 id_mmfr1_el1, uint32 id_mmfr2_el1,
                                    uint32 id_mmfr3_el1, uint32 id_mmfr4_el1, uint32 id_mmfr5_el1);
    bool setup_aarch32_media_vfp(uint32 mvfr0_el1, uint32 mvfr1_el1, uint32 mvfr2_el1,
                                 uint64 midr_el1);
    bool setup_aarch32_debug(uint64 id_aa64dfr0_el1, uint32 id_dfr0_el1);

    virtual bool setup_page_table_regs(Vbus::Bus&);
    bool setup_tvm(Vbus::Bus&);
    bool setup_gic_registers(Model::GicD&);

protected:
    bool register_system_reg(RegisterBase* reg) {
        ASSERT(reg != nullptr);
        return register_device(reg, reg->id(), sizeof(uint64));
    }

public:
    bool setup_aarch64_physical_timer(Model::AA64Timer& ptimer);

    struct CacheTopo {
        uint64 ctr_el0;
        uint64 clidr_el1;
        uint64 ccsidr_el1[CCSIDR_NUM * 2];
    };

    bool setup_arch_msr(const PlatformInfo& info, Vbus::Bus&, Model::GicD&);
    bool setup_aarch64_caching_info(const CacheTopo& topo);

    RegisterBase* get_register_with_id(Msr::Id id) const {
        return reinterpret_cast<RegisterBase*>(get_device_at(id.id(), sizeof(uint64)));
    }
};
