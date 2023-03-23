/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <intrusive/map.hpp>
#include <model/aa64_timer.hpp>
#include <model/cpu.hpp>
#include <model/vcpu_types.hpp>
#include <msr/msr_access.hpp>
#include <msr/msr_id.hpp>
#include <platform/log.hpp>
#include <platform/reg_accessor.hpp>
#include <platform/time.hpp>

namespace Msr {

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
    class Set_way_flush_reg;
    class WtrappedMsr;
    class SctlrEl1;
    class MdscrEl1;

    using Vbus::Err;

    constexpr uint8 CCSIDR_NUM{7};

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
        DCCSW_A64 = build_msr_id(0b01, 0b0111, 0b000, 0b1010, 0b010),
        DCCISW_A32 = build_msr_id(0b1111, 0b0111, 0b000, 0b1110, 0b010),
        DCCISW_A64 = build_msr_id(0b01, 0b0111, 0b000, 0b1110, 0b010),
        MVFR0 = build_msr_id(3, 0, 0, 3, 0),
        MVFR1 = build_msr_id(3, 0, 0, 3, 1),
        MVFR2 = build_msr_id(3, 0, 0, 3, 2),
        CONTEXTIDR_A32 = build_msr_id(0b1111, 0xd, 0, 0, 1),
        CONTEXTIDR_EL1 = build_msr_id(3, 0xd, 0, 0, 1),
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
        ID_AA64SMFR0_EL1 = Msr::build_msr_id(3, 0, 0, 4, 5),
        CCSIDR_EL1 = Msr::build_msr_id(3, 0, 1, 0, 0),
        ID_PFR0_EL1 = Msr::build_msr_id(3, 0, 0, 1, 0),
        ID_PFR1_EL1 = Msr::build_msr_id(3, 0, 0, 1, 1),
        ID_AA64DFR0_EL1 = build_msr_id(3, 0, 0, 5, 0),
        ID_AA64DFR1_EL1 = build_msr_id(3, 0, 0, 5, 1),
        MDSCR_EL1 = build_msr_id(2, 0, 0, 2, 2),

        DCIVAC = build_msr_id(1, 7, 0, 6, 1),
        DCCVAC = build_msr_id(1, 7, 3, 10, 1),
        DCCVAU = build_msr_id(1, 7, 3, 11, 1),
        DCCVAP = build_msr_id(1, 7, 3, 12, 1),
        DCCIVAC = build_msr_id(1, 7, 3, 14, 1),
        DCZVA = build_msr_id(1, 7, 3, 4, 1),

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
        ID_AA64ISAR2_EL1 = build_msr_id(3, 0, 0, 6, 2),
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
        OSDLR_EL1 = build_msr_id(2, 1, 0, 3, 4),
        OSLAR_EL1 = build_msr_id(2, 1, 0, 0, 4),
        OSLSR_EL1 = build_msr_id(2, 1, 0, 1, 4),
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
        VMPIDR_EL2 = build_msr_id(0b11, 0b0000, 0b100, 0b0000, 0b101),
        ESR_EL2 = build_msr_id(0b11, 0b0101, 0b100, 0b0010, 0b000),
        ELR_EL2 = build_msr_id(0b11, 0b0100, 0b100, 0b0000, 0b001),
        ELR_EL1 = build_msr_id(0b11, 0b0100, 0b000, 0b0000, 0b001),
        FAR_EL2 = build_msr_id(0b11, 0b0110, 0b100, 0b0000, 0b000),
        SPSR_EL2 = build_msr_id(0b11, 0b0100, 0b100, 0b0000, 0b000),
        HCR_EL2 = build_msr_id(0b11, 0b0001, 0b100, 0b0001, 0b000),
        SCTLR_EL2 = build_msr_id(0b11, 0b0001, 0b100, 0b0000, 0b000),
        DAIF = build_msr_id(0b11, 0b0100, 0b011, 0b0010, 0b001),
        SP_EL0 = build_msr_id(0b11, 0b0100, 0b000, 0b0001, 0b000),

        // RAS registers
        ERRIDR_EL1 = build_msr_id(3, 5, 0, 3, 0),
        ERRSELR_EL1 = build_msr_id(3, 5, 0, 3, 1),
        ERXADDR_EL1 = build_msr_id(3, 5, 0, 4, 3),
        ERXCTLR_EL1 = build_msr_id(3, 5, 0, 4, 1),
        ERXFR_EL1 = build_msr_id(3, 5, 0, 4, 0),
        ERXMISC0_EL1 = build_msr_id(3, 5, 0, 5, 0),
        ERXMISC1_EL1 = build_msr_id(3, 5, 0, 5, 1),
        ERXSTATUS_EL1 = build_msr_id(3, 5, 0, 4, 2),

        // PMS registers
        PMSCR_EL1 = build_msr_id(3, 9, 0, 9, 0),
        PMSEVFR_EL1 = build_msr_id(3, 9, 0, 9, 5),
        PMSFCR_EL1 = build_msr_id(3, 9, 0, 9, 4),
        PMSICR_EL1 = build_msr_id(3, 9, 0, 4, 2),
        PMSIDR_EL1 = build_msr_id(3, 9, 0, 4, 7),
        PMSIRR_EL1 = build_msr_id(3, 9, 0, 5, 3),
        PMSLATFR_EL1 = build_msr_id(3, 5, 0, 5, 6),

        // PM registers
        PMCR_EL0 = build_msr_id(3, 9, 3, 12, 0),
        PMCNTENSET_EL0 = build_msr_id(3, 9, 3, 12, 1),
        PMCNTENCLR_EL0 = build_msr_id(3, 9, 3, 12, 2),
        PMOVSCLR_EL0 = build_msr_id(3, 9, 3, 12, 3),
        PMSWINC_EL0 = build_msr_id(3, 9, 3, 12, 4),
        PMSELR_EL0 = build_msr_id(3, 9, 3, 12, 5),
        PMCEID0_EL0 = build_msr_id(3, 9, 3, 12, 6),
        PMCEID1_EL0 = build_msr_id(3, 9, 3, 12, 7),
        PMCCNTR_EL0 = build_msr_id(3, 9, 3, 13, 0),
        PMXEVTYPER_EL0 = build_msr_id(3, 9, 3, 13, 1),
        PMXEVCNTR_EL0 = build_msr_id(3, 9, 3, 13, 2),
        PMUSERENR_EL0 = build_msr_id(3, 9, 3, 14, 0),
        PMOVSSET_EL0 = build_msr_id(3, 9, 3, 14, 3),
        PMCCFILTR_EL0 = build_msr_id(3, 14, 3, 15, 7),
        PMINTENSET_EL1 = build_msr_id(3, 9, 0, 14, 1),
        PMINTENCLR_EL1 = build_msr_id(3, 9, 0, 14, 2),

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
        = {DBGBVR0_EL1, DBGBVR1_EL1, DBGBVR2_EL1,  DBGBVR3_EL1,  DBGBVR4_EL1,  DBGBVR5_EL1,  DBGBVR6_EL1,  DBGBVR7_EL1,
           DBGBVR8_EL1, DBGBVR9_EL1, DBGBVR10_EL1, DBGBVR11_EL1, DBGBVR12_EL1, DBGBVR13_EL1, DBGBVR14_EL1, DBGBVR15_EL1};

    static constexpr uint32 DBGBCR_EL1[]
        = {DBGBCR0_EL1, DBGBCR1_EL1, DBGBCR2_EL1,  DBGBCR3_EL1,  DBGBCR4_EL1,  DBGBCR5_EL1,  DBGBCR6_EL1,  DBGBCR7_EL1,
           DBGBCR8_EL1, DBGBCR9_EL1, DBGBCR10_EL1, DBGBCR11_EL1, DBGBCR12_EL1, DBGBCR13_EL1, DBGBCR14_EL1, DBGBCR15_EL1};

    static constexpr uint32 DBGWVR_EL1[]
        = {DBGWVR0_EL1, DBGWVR1_EL1, DBGWVR2_EL1,  DBGWVR3_EL1,  DBGWVR4_EL1,  DBGWVR5_EL1,  DBGWVR6_EL1,  DBGWVR7_EL1,
           DBGWVR8_EL1, DBGWVR9_EL1, DBGWVR10_EL1, DBGWVR11_EL1, DBGWVR12_EL1, DBGWVR13_EL1, DBGWVR14_EL1, DBGWVR15_EL1};

    static constexpr uint32 DBGWCR_EL1[]
        = {DBGWCR0_EL1, DBGWCR1_EL1, DBGWCR2_EL1,  DBGWCR3_EL1,  DBGWCR4_EL1,  DBGWCR5_EL1,  DBGWCR6_EL1,  DBGWCR7_EL1,
           DBGWCR8_EL1, DBGWCR9_EL1, DBGWCR10_EL1, DBGWCR11_EL1, DBGWCR12_EL1, DBGWCR13_EL1, DBGWCR14_EL1, DBGWCR15_EL1};

    static constexpr uint8 NUM_PMEVCNTR_REGS = 31;
    static constexpr uint32 pmevcntrn_el0(uint8 id) {
        return build_msr_id(3, 14, 3, static_cast<uint8>((0b10 << 2) | ((id >> 3) & 0b11)), id & 0b111);
    }

    static constexpr uint8 NUM_PMEVTYPER_REGS = 31;
    static constexpr uint32 pmevtypern_el0(uint8 id) {
        return build_msr_id(3, 14, 3, static_cast<uint8>((0b11 << 2) | ((id >> 3) & 0b11)), id & 0b111);
    }
}

namespace Model {
    class GicD;
}

class Msr::RegisterBase : public Map_key<mword> {
public:
    RegisterBase(const char* name, Id reg_id) : Map_key(), _name(name), _reg_id(reg_id) {}

    virtual Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, uint64& res) = 0;

    uint32 id() const { return _reg_id.id(); };

    /*! \brief Reset the device to its initial state
     *  \pre The caller has full ownership of a valid Device object which can be in any state.
     *  \post The ownership of the object is returned to the caller. The device is in its initial
     *        state.
     */
    virtual void reset(const VcpuCtx* vcpu_ctx) = 0;

    /*! \brief Query the name of the device
     *  \pre The caller has partial ownership of a valid Device object
     *  \post A pointer pointing to a valid array of chars representing the name of the this device.
     *        The caller also receives a fractional ownership of the name.
     *  \return the name of the device
     */
    const char* name() const { return _name; }

private:
    const char* _name; /*!< Name of the device - cannot be changed at run-time */
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
    Register(const char* name, Id const reg_id, bool const writable, uint64 const reset_value, uint64 const mask = ~0ULL)
        : RegisterBase(name, reg_id), _value(reset_value), _reset_value(reset_value), _write_mask(mask), _writable(writable) {}

    virtual Err access(Vbus::Access access, const VcpuCtx*, uint64& value) override {
        if (access == Vbus::WRITE && !_writable)
            return Err::ACCESS_ERR;

        if (access == Vbus::WRITE) {
            _value |= value & _write_mask;                    // Set the bits at 1
            _value &= ~(_write_mask & (value ^ _write_mask)); // Set the bits at 0
        } else {
            value = _value;
        }

        return Err::OK;
    }

    virtual void reset(const VcpuCtx*) override { _value = _reset_value; }
};

class Msr::Set_way_flush_reg : public Msr::Register {
public:
    Set_way_flush_reg(const char* name, Id const reg_id, Vbus::Bus& b)
        : Register(name, reg_id, true, 0x0, 0x00000000fffffffeull) {
        if (vbus == nullptr)
            vbus = &b;
    }

    virtual Err access(Vbus::Access access, const VcpuCtx* vctx, uint64& value) override {
        Err ret = Register::access(access, vctx, value);

        if (access == Vbus::Access::WRITE) {
            flush(vctx, (_value >> 1) & 0x7, static_cast<uint32>(_value >> 4));
        }

        return ret;
    }

    static Vbus::Bus* get_associated_bus() { return vbus; }

protected:
    void flush(const VcpuCtx*, uint8, uint32) const;

    static Vbus::Bus* vbus;
};

class Msr::IdAa64pfr0 : public Register {
private:
    uint64 _reset_value(uint64 value) const {
        value &= ~(0xfull << 32); /* sve - not implemented */
        value &= ~(0xfull << 40); /* MPAM - not implemented */
        value &= ~(0xfull << 44); /* AMU - not implemented */
        return value;
    }

public:
    explicit IdAa64pfr0(uint64 value) : Register("ID_AA64PFR0_EL1", ID_AA64PFR0_EL1, false, _reset_value(value)) {}
};

class Msr::IdPfr0 : public Register {
private:
    uint64 _reset_value(uint32 value) const {
        /* Take the value of the HW for state 0 to 3 (bits[15:0]), the rest is not implemented */
        return value & 0xffffull;
    }

public:
    explicit IdPfr0(uint32 value) : Register("ID_PFR0_EL1", ID_PFR0_EL1, false, _reset_value(value)) {}
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
    explicit IdPfr1(uint32 value) : Register("ID_PFR1_EL1", ID_PFR1_EL1, false, _reset_value(value)) {}
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

    virtual Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, uint64& value) override {
        if (access == Vbus::WRITE)
            return Err::ACCESS_ERR;

        uint64 el = 0;
        if (Err::OK != _csselr.access(access, vcpu_ctx, el))
            return Err::ACCESS_ERR;

        bool const instr = (el & 0x1) != 0u;
        unsigned const level = (el >> 1) & 0x7;

        if (level > 6)
            return Err::ACCESS_ERR;

        uint8 const ce = (_clidr_el1 >> (level * 3)) & 0b111;

        if (ce == NO_CACHE || (ce == DATA_CACHE_ONLY && instr)) {
            value = INVALID;
            return Err::OK;
        }
        if (ce == INSTRUCTION_CACHE_ONLY || (ce == SEPARATE_CACHE && instr)) {
            value = _ccsidr_inst_el1[level];
            return Err::OK;
        }

        value = _ccsidr_data_el1[level];
        return Err::OK;
    }

    virtual void reset(const VcpuCtx*) override {}
};

class Msr::IccSgi1rEl1 : public RegisterBase {
private:
    Model::GicD* _gic;

public:
    explicit IccSgi1rEl1(Model::GicD& gic) : RegisterBase("ICC_SGI1R_EL1", ICC_SGI1R_EL1), _gic(&gic) {}

    virtual Err access(Vbus::Access, const VcpuCtx*, uint64&) override;

    virtual void reset(const VcpuCtx*) override {}
};

class Msr::CntpCtl : public Register {
private:
    Model::AA64Timer* _ptimer;

public:
    CntpCtl(const char* name, Msr::RegisterId id, Model::AA64Timer& t) : Register(name, id, true, 0, 0b11), _ptimer(&t) {}

    virtual Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, uint64& value) {
        _value = _ptimer->get_ctl();

        Err err = Register::access(access, vcpu_ctx, value);
        if (err == Err::OK && access == Vbus::WRITE) {
            _ptimer->set_ctl(static_cast<uint8>(_value));
        }

        return err;
    }
};

class Msr::CntpCval : public Register {
private:
    Model::AA64Timer* _ptimer;

public:
    CntpCval(const char* name, Msr::RegisterId id, Model::AA64Timer& t) : Register(name, id, true, 0), _ptimer(&t) {}

    virtual Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, uint64& value) {
        _value = _ptimer->get_cval();

        Err err = Register::access(access, vcpu_ctx, value);
        if (err == Msr::Err::OK && access == Vbus::WRITE) {
            _ptimer->set_cval(_value);
        }

        return err;
    }
};

class Msr::WtrappedMsr : public Msr::RegisterBase {
public:
    using Msr::RegisterBase::RegisterBase;

    virtual Err access(Vbus::Access access, const VcpuCtx*, uint64&) override {
        ASSERT(access == Vbus::Access::WRITE); // We only trap writes at the moment
        return Err::UPDATE_REGISTER;           // Tell the VCPU to update the relevant physical
                                               // register
    }
    virtual void reset(const VcpuCtx*) override {}
};

class Msr::SctlrEl1 : public Msr::RegisterBase {
public:
    SctlrEl1(const char* name, Msr::Id reg_id) : Msr::RegisterBase(name, reg_id) {}

    virtual Err access(Vbus::Access access, const VcpuCtx* vcpu, uint64& res) override;
    virtual void reset(const VcpuCtx*) override {}
};

class Msr::MdscrEl1 : public Msr::Register {
private:
    static constexpr uint64 MDSCREL1_SS = 0x1ull;
    bool mdscr_ss_enabled(uint64 value) const { return (value & MDSCREL1_SS) != 0; }

public:
    MdscrEl1() : Msr::Register("MDSCR_EL1", MDSCR_EL1, true, 0x0ULL) {}

    virtual Err access(Vbus::Access access, const VcpuCtx* vcpu, uint64& value) override {

        if (access == Vbus::WRITE && mdscr_ss_enabled(value)) {
            if (!mdscr_ss_enabled(_value)) {
                WARN("Guest as enabled software step control bit which is not supported");
            }
        }

        return Msr::Register::access(access, vcpu, value);
    }
};

/*
 * This is the bus that will handle all reads and writes
 * to system registers.
 */
class Msr::Bus {
public:
    Bus() {}

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

    /*! \brief Add a register to the msr bus
     *  \pre Full ownership of a valid virtual bus. Full ownership of a valid register.
     *  \post Ownership of the bus is unchanged. The bus adds this register to its
     *        internal list if there is no conflict. Otherwise, ownership of the register
     *        is returned and no changes are performed on the bus.
     *  \param r RegisterBase to add
     *  \return true if there is no conflict and the register was added. false otherwise.
     */
    [[nodiscard]] bool register_device(RegisterBase* r, mword id);

    /*! \brief Query for a register with given id
     *  \pre Fractional ownership of a valid msr bus.
     *  \post Ownership of the bus is unchanged. The bus itself is not changed.
     *        If a register was found, a fractional ownership to a valid RegisterBase is returned.
     *  \param id Id of the device in the Bus
     *  \return nullptr is no register with that id, the register otherwise.
     */
    RegisterBase* get_device_at(mword id) const { return _devices[id]; }

    /*! \brief Access the register at the given location
     *  \pre Fractional ownership of a valid bus. Full ownership of a valid Vcpu context.
     *  \post Ownership of the bus is unchanged. Ownership of the Vcpu context is returned.
     *        If a register was found at the given range, access was called on that register.
     *        An status is returned to understand the consequence of the call.
     *  \param access Type of access (R/W/X)
     *  \param vcpu_ctx Contains information about the VCPU generating this access
     *  \param id Id of the device in the Bus
     *  \param val Buffer for read or write operations
     *  \return The status of the access
     */
    Err access(Vbus::Access access, const VcpuCtx& vcpu_ctx, mword id, uint64& val);

    /*! \brief Reset all registers on the bus
     *  \pre Fractional ownership of a valid msr bus.
     *  \post Ownership of the bus is unchanged. All registers must have transitioned from some
     *        state to their initial state.
     */
    void reset(const VcpuCtx& vcpu_ctx);

    /*! \brief Debug only: control the trace of the access to the bus
     *  \param enabled Should accesses be traced?
     *  \param fold_successive Should repeated accesses to the same register be logged only once?
     */
    void set_trace(bool enabled, bool fold_successive) {
        _trace = enabled;
        _fold = fold_successive;
    }

private:
    bool setup_aarch64_features(uint64 id_aa64pfr0_el1, uint64 id_aa64pfr1_el1, uint64 id_aa64isar0_el1, uint64 id_aa64isar1_el1,
                                uint64 id_aa64isar2_el1, uint64 id_aa64zfr0_el1);
    bool setup_aarch64_setway_flushes(Vbus::Bus& vbus);
    bool setup_aarch64_memory_model(uint64 id_aa64mmfr0_el1, uint64 id_aa64mmfr1_el1, uint64 id_aa64mmfr2_el1);
    bool setup_aarch64_debug(uint64 id_aa64dfr0_el1, uint64 id_aa64dfr1_el1);
    bool setup_aarch64_auxiliary();
    bool setup_aarch64_ras();
    bool setup_aarch64_pm();
    bool setup_aarch64_pms();

    bool setup_aarch32_msr(const PlatformInfo& info);
    bool setup_aarch32_features(const AA32PlatformInfo& aa32);
    bool setup_aarch32_memory_model(uint32 id_mmfr0_el1, uint32 id_mmfr1_el1, uint32 id_mmfr2_el1, uint32 id_mmfr3_el1,
                                    uint32 id_mmfr4_el1, uint32 id_mmfr5_el1);
    bool setup_aarch32_media_vfp(uint32 mvfr0_el1, uint32 mvfr1_el1, uint32 mvfr2_el1, uint64 midr_el1);
    bool setup_aarch32_debug(uint64 id_aa64dfr0_el1, uint32 id_dfr0_el1);

    virtual bool setup_page_table_regs();
    bool setup_tvm();
    bool setup_gic_registers(Model::GicD&);

    static void reset_register_cb(Msr::RegisterBase*, const VcpuCtx*);

    void log_trace_info(const RegisterBase* reg, Vbus::Access access, uint64 val);

    Map_kv<mword, RegisterBase> _devices;
    bool _trace{false};
    bool _fold{true};
    const RegisterBase* _last_access{nullptr};
    size_t _num_accesses{0};

protected:
    bool register_system_reg(RegisterBase* reg) {
        ASSERT(reg != nullptr);
        bool ret;

        ret = register_device(reg, reg->id());
        if (!ret) {
            Msr::RegisterBase* r = get_register_with_id(reg->id());
            if (r != nullptr) {
                ABORT_WITH("Trying to register %s, but, ID is used by %s", reg->name(), r->name());
            } else {
                ABORT_WITH("Unable to register %s, allocation failure", reg->name());
            }
        }

        return true;
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

    RegisterBase* get_register_with_id(Msr::Id id) const { return reinterpret_cast<RegisterBase*>(get_device_at(id.id())); }
};
