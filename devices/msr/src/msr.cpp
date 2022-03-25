/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/aa64_timer.hpp>
#include <model/cpu.hpp>
#include <model/gic.hpp>
#include <model/simple_as.hpp>
#include <msr/msr.hpp>
#include <msr/msr_info.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <platform/types.hpp>

static constexpr uint64 AA64DFR0_DEBUG_V8 = 0x6ull;
Vbus::Bus *Msr::Set_way_flush_reg::vbus = nullptr;

bool
Msr::Bus::setup_aarch64_debug(uint64 id_aa64dfr0_el1, uint64 id_aa64dfr1_el1) {
    Msr::Register *reg;

    id_aa64dfr0_el1 = AA64DFR0_DEBUG_V8;
    reg = new (nothrow) Msr::Register("ID_AA64DFR0_EL1", ID_AA64DFR0_EL1, false, id_aa64dfr0_el1);
    if (!register_system_reg(reg))
        return false;

    id_aa64dfr1_el1 = 0ull;
    reg = new (nothrow) Msr::Register("ID_AA64DFR1_EL1", ID_AA64DFR1_EL1, false, id_aa64dfr1_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("MDSCR_EL1", MDSCR_EL1, true, 0x0ULL);
    if (!register_system_reg(reg))
        return false;

    for (uint8 i = 0; i < 16; i++) {
        reg = new (nothrow) Msr::Register("DBGBVR_EL1", DBGBVR_EL1[i], true, 0x0ULL, 0x0ULL);
        if (!register_system_reg(reg))
            return false;

        reg = new (nothrow) Msr::Register("DBGBCR_EL1", DBGBCR_EL1[i], true, 0x0ULL, 0x0ULL);
        if (!register_system_reg(reg))
            return false;

        reg = new (nothrow) Msr::Register("DBGWVR_EL1", DBGWVR_EL1[i], true, 0x0ULL, 0x0ULL);
        if (!register_system_reg(reg))
            return false;

        reg = new (nothrow) Msr::Register("DBGWCR_EL1", DBGWCR_EL1[i], true, 0x0ULL, 0x0ULL);
        if (!register_system_reg(reg))
            return false;
    }

    reg = new (nothrow) Msr::Register("MDRAR_EL1", MDRAR_EL1, true, 0x0ULL);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_aarch32_debug(uint64 id_aa64dfr0_el1, uint32 id_dfr0_el1) {
    Msr::Register *reg;

    /*
     * XXX: Slight abuse of the spec. We shouldn't be allowed to disable monitoring
     * and debugging in ARMv8.0a running in aarch32. However, the VMM doesn't handle
     * the emulation of debug and perf registers at the moment.
     */
    id_dfr0_el1 = 0;
    reg = new (nothrow) Msr::Register("ID_DFR0_EL1", ID_DFR0_EL1, false, id_dfr0_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_DFR1_EL1", ID_DFR1_EL1, false, 0x0ull);
    if (!register_system_reg(reg))
        return false;

    Msr::Info::IdAa64dfr0 aa64dfr0(id_aa64dfr0_el1);

    /*
     * XXX: same reason as above. We don't implement pmu and debug features for now.
     * When we do, the next statement can be removed and the rest should work.
     * uint32 dbgidr = static_cast<uint32>(aa64dfr0.wrp() << 28u)
     *                 | static_cast<uint32>(aa64dfr0.brp() << 24u)
     *                 | static_cast<uint32>(aa64dfr0.ctx_cmp() << 20u)
     *                 | static_cast<uint32>(aa64dfr0.debug_ver() << 16u) | 1u << 15u;
     */
    uint32 dbgidr = 0;
    reg = new (nothrow) Msr::Register("DBGDIDR", DBGDIDR, false, dbgidr);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_aarch64_auxiliary() {
    Msr::Register *reg;

    reg = new (nothrow) Msr::Register("ACTLR_EL1", ACTLR_EL1, false, 0x0ull);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("DBGAUTHSTATUS_EL1", DBGAUTHSTATUS_EL1, false, 0x0ull);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_aarch64_features(uint64 id_aa64pfr0_el1, uint64 id_aa64pfr1_el1,
                                 uint64 id_aa64isar0_el1, uint64 id_aa64isar1_el1,
                                 uint64 id_aa64zfr0_el1) {
    Msr::Register *reg;

    reg = new (nothrow) Msr::IdAa64pfr0(id_aa64pfr0_el1);
    if (!register_system_reg(reg))
        return false;

    id_aa64pfr1_el1 = 0ull;
    reg = new (nothrow) Msr::Register("ID_AA64PFR1_EL1", ID_AA64PFR1_EL1, false, id_aa64pfr1_el1);
    if (!register_system_reg(reg))
        return false;

    id_aa64zfr0_el1 = 0ull;
    reg = new (nothrow) Msr::Register("ID_AA64ZFR0_EL1", ID_AA64ZFR0_EL1, false, id_aa64zfr0_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow)
        Msr::Register("ID_AA64ISAR0_EL1", ID_AA64ISAR0_EL1, false, id_aa64isar0_el1);
    if (!register_system_reg(reg))
        return false;

    /*
     * PAuth is not yet implemented in the VMM. Removing this feature in case the host
     * exposes it
     */
    id_aa64isar1_el1 &= ~(0xffull << 4);  // APA, API
    id_aa64isar1_el1 &= ~(0xffull << 24); // GPA, GPI

    reg = new (nothrow)
        Msr::Register("ID_AA64ISAR1_EL1", ID_AA64ISAR1_EL1, false, id_aa64isar1_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_AA64AFR0_EL1", ID_AA64AFR0_EL1, false, 0x0ULL);
    if (!register_system_reg(reg))
        return false;
    reg = new (nothrow) Msr::Register("ID_AA64AFR1_EL1", ID_AA64AFR1_EL1, false, 0x0ULL);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_aarch64_ras() {
    Msr::Register *reg;

    reg = new (nothrow) Msr::Register("ERRIDR_EL1", ERRIDR_EL1, false, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ERRSELR_EL1", ERRSELR_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ERXADDR_EL1", ERXADDR_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ERXCTLR_EL1", ERXCTLR_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ERXFR_EL1", ERXFR_EL1, false, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ERXSTATUS_EL1", ERXSTATUS_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ERXMISC0_EL1", ERXMISC0_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ERXMISC1_EL1", ERXMISC1_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    return true;
}

bool
Msr::Bus::setup_aarch64_pms() {
    Msr::Register *reg;

    // Strict minimum when it comes to features implemented
    uint64 idr = 0b0010 << 16 | 0b0110 << 12 | 0b111;
    reg = new (nothrow) Msr::Register("PMSIDR_EL1", PMSIDR_EL1, false, idr);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("PMSCR_EL1", PMSCR_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    // Ignore writes for this register
    reg = new (nothrow) Msr::Register("PMSEVFR_EL1", PMSEVFR_EL1, true, 0, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("PMSICR_EL1", PMSICR_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("PMSIRR_EL1", PMSIRR_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("PMSLATFR_EL1", PMSLATFR_EL1, true, 0);
    if (!register_system_reg(reg))
        return false;

    return true;
}

bool
Msr::Bus::setup_aarch32_features(const AA32PlatformInfo &aa32) {
    Msr::Register *reg;

    reg = new (nothrow) Msr::IdPfr0(aa32.id_pfr0_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::IdPfr1(aa32.id_pfr1_el1);
    if (!register_system_reg(reg))
        return false;

    uint32 id_pfr2_el1 = 0x0ul; /* Nothing implemented for ARMv8.0-a */
    reg = new (nothrow) Msr::Register("ID_PFR2_EL1", ID_PFR2_EL1, false, id_pfr2_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_ISAR0_EL1", ID_ISAR0_EL1, false, aa32.id_isar0_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_ISAR1_EL1", ID_ISAR1_EL1, false, aa32.id_isar1_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_ISAR2_EL1", ID_ISAR2_EL1, false, aa32.id_isar2_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_ISAR3_EL1", ID_ISAR3_EL1, false, aa32.id_isar3_el1);
    if (!register_system_reg(reg))
        return false;

    uint32 id_isar4_el1 = aa32.id_isar4_el1
                          & ~(0xfu << 12); /* SMC has to be zero if we don't support aarch32 el1 */
    reg = new (nothrow) Msr::Register("ID_ISAR4_EL1", ID_ISAR4_EL1, false, id_isar4_el1);
    if (!register_system_reg(reg))
        return false;

    uint32 id_isar5_el1
        = aa32.id_isar5_el1 & 0xffffful; /* Only the bits[19:0] can have a meaning for ARMv8.0-A */
    reg = new (nothrow) Msr::Register("ID_ISAR5_EL1", ID_ISAR5_EL1, false, id_isar5_el1);
    if (!register_system_reg(reg))
        return false;

    uint32 id_isar6_el1 = 0ull; /* Read as Zero before ARMv8.2 */
    reg = new (nothrow) Msr::Register("ID_ISAR6_EL1", ID_ISAR6_EL1, false, id_isar6_el1);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_aarch64_memory_model(uint64 id_aa64mmfr0_el1, uint64 id_aa64mmfr1_el1,
                                     uint64 id_aa64mmfr2_el1) {
    Msr::Register *reg;

    id_aa64mmfr0_el1 &= ~(0xfull << 60); /* Enhanced Virt counter is disabled */
    reg = new (nothrow) Msr::Register("ID_AA64MMFR0_EL1", Msr::RegisterId::ID_AA64MMFR0_EL1, false,
                                      id_aa64mmfr0_el1);
    if (!register_system_reg(reg))
        return false;

    id_aa64mmfr1_el1 &= ~(0xfull << 8);  /* Virtualization Host Extension is disabled */
    id_aa64mmfr1_el1 &= ~(0xfull << 16); /* LORegions not supported */
    reg = new (nothrow)
        Msr::Register("ID_AA64MMFR1_EL1", ID_AA64MMFR1_EL1, false, id_aa64mmfr1_el1);
    if (!register_system_reg(reg))
        return false;

    id_aa64mmfr2_el1 &= ~(0xfull << 24); /* Nested Virtualization is disabled */
    id_aa64mmfr2_el1 &= ~(0xfull << 56); /* Enhanced Virtualization Traps is disabled */
    reg = new (nothrow)
        Msr::Register("ID_AA64MMFR2_EL1", ID_AA64MMFR2_EL1, false, id_aa64mmfr2_el1);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_aarch32_memory_model(uint32 id_mmfr0_el1, uint32 id_mmfr1_el1, uint32 id_mmfr2_el1,
                                     uint32 id_mmfr3_el1, uint32 id_mmfr4_el1,
                                     uint32 id_mmfr5_el1) {
    Msr::Register *reg;

    reg = new (nothrow) Msr::Register("ID_MMFR0_EL1", ID_MMFR0_EL1, false, id_mmfr0_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_MMFR1_EL1", ID_MMFR1_EL1, false, id_mmfr1_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_MMFR2_EL1", ID_MMFR2_EL1, false, id_mmfr2_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_MMFR3_EL1", ID_MMFR3_EL1, false, id_mmfr3_el1);
    if (!register_system_reg(reg))
        return false;

    id_mmfr4_el1 = 0u; /* Only contains features that we don't implement */
    reg = new (nothrow) Msr::Register("ID_MMFR4_EL1", ID_MMFR4_EL1, false, id_mmfr4_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_MMFR5_EL1", ID_MMFR5_EL1, false, id_mmfr5_el1);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_aarch64_setway_flushes(Vbus::Bus &vbus) {
    Msr::Register *reg;

    reg = new (nothrow) Msr::Set_way_flush_reg("DC ISW", DCISW_A64, vbus);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Set_way_flush_reg("DC CSW", DCCSW_A64, vbus);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Set_way_flush_reg("DC CISW", DCCISW_A64, vbus);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_aarch64_caching_info(const CacheTopo &topo) {
    Msr::Register *reg, *csselr_el1;

    reg = new (nothrow) Msr::Register("CLIDR_EL1", CLIDR_EL1, false, topo.clidr_el1);
    if (!register_system_reg(reg))
        return false;

    csselr_el1 = new (nothrow) Msr::Register("CSSELR_EL1", CSSELR_EL1, true, 0x0ULL);
    if (!register_system_reg(csselr_el1))
        return false;

    reg = new (nothrow) Msr::Register("CTR_EL0", Msr::RegisterId::CTR_A64, false, topo.ctr_el0);
    if (!register_system_reg(reg))
        return false;

    Msr::Ccsidr *ccsidr = new (nothrow) Msr::Ccsidr(*csselr_el1, topo.clidr_el1, topo.ccsidr_el1);
    return register_system_reg(ccsidr);
}

bool
Msr::Bus::setup_aarch32_media_vfp(uint32 mvfr0_el1, uint32 mvfr1_el1, uint32 mvfr2_el1,
                                  uint64 midr_el1) {
    constexpr uint32 FPSID_VFP_VERSION_3_NULL_SUB = 0b0000011;
    uint32 fpsid
        = (static_cast<uint32>(midr_el1) & 0xf0000000) | FPSID_VFP_VERSION_3_NULL_SUB << 16;
    Msr::Register *reg;

    reg = new (nothrow) Msr::Register("FPSID", FPSID, true, fpsid);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_MVFR0_EL1", Msr::MVFR0, false, mvfr0_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_MVFR1_EL1", Msr::MVFR1, false, mvfr1_el1);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::Register("ID_MVFR2_EL1", Msr::MVFR2, false, mvfr2_el1);
    return register_system_reg(reg);
}

bool
Msr::Bus::setup_aarch64_physical_timer(Model::AA64Timer &ptimer) {
    Msr::Register *reg;

    reg = new (nothrow) Msr::CntpTval("CNTP_TVAL_EL0", Msr::RegisterId::CNTP_TVAL_EL0, ptimer);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::CntpCtl("CNTP_CTL_EL0", Msr::RegisterId::CNTP_CTL_EL0, ptimer);
    if (!register_system_reg(reg))
        return false;

    reg = new (nothrow) Msr::CntpCval("CNTP_CVAL_EL0", Msr::RegisterId::CNTP_CVAL_EL0, ptimer);
    if (!register_system_reg(reg))
        return false;

    Msr::CntpctEl0 *cntpct = new (nothrow) Msr::CntpctEl0();
    return register_system_reg(cntpct);
}

bool
Msr::Bus::setup_page_table_regs() {
    bool ok = false;

    ok = register_system_reg(new (nothrow) WtrappedMsr("TCR_EL1", Msr::Id(TCR_EL1)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("TTBR0_EL1", Msr::Id(TTBR0_EL1)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("TTBR1_EL1", Msr::Id(TTBR1_EL1)));
    if (!ok)
        return false;
    return register_system_reg(new (nothrow) SctlrEl1("SCTLR_EL1", Msr::Id(SCTLR_EL1)));
}

bool
Msr::Bus::setup_tvm() {
    bool ok = false;

    ok = register_system_reg(new (nothrow) WtrappedMsr("AFSR0_EL1", Msr::Id(AFSR0_EL1)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("AFSR1_EL1", Msr::Id(AFSR1_EL1)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("ESR_EL1", Msr::Id(ESR_EL1)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("FAR_EL1", Msr::Id(FAR_EL1)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("MAIR_EL1", Msr::Id(MAIR_EL1)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("MAIR1_A32", Msr::Id(Msr::MAIR1_A32)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("AMAIR_EL1", Msr::Id(AMAIR_EL1)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("DACR", Msr::Id(Msr::DACR)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("IFSR", Msr::Id(Msr::IFSR)));
    if (!ok)
        return false;
    ok = register_system_reg(new (nothrow) WtrappedMsr("CONTEXTDIR_EL1", Msr::Id(CONTEXTIDR_EL1)));
    if (!ok)
        return false;

    return setup_page_table_regs();
}

bool
Msr::Bus::setup_gic_registers(Model::GicD &gicd) {
    Msr::IccSgi1rEl1 *sgi1r = new (nothrow) Msr::IccSgi1rEl1(gicd);
    return register_system_reg(sgi1r);
}

bool
Msr::Bus::setup_aarch32_msr(const PlatformInfo &info) {
    Msr::Register *reg;

    if (!setup_aarch32_features(info.aa32))
        return false;

    if (!setup_aarch32_memory_model(info.aa32.id_mmfr0_el1, info.aa32.id_mmfr1_el1,
                                    info.aa32.id_mmfr2_el1, info.aa32.id_mmfr3_el1,
                                    info.aa32.id_mmfr4_el1, info.aa32.id_mmfr5_el1))
        return false;

    if (!setup_aarch32_media_vfp(info.aa32.mvfr0_el1, info.aa32.mvfr1_el1, info.aa32.mvfr2_el1,
                                 info.aa64.midr_el1))
        return false;

    if (!setup_aarch32_debug(info.aa64.id_aa64dfr0_el1, info.aa32.id_dfr0_el1))
        return false;

    reg = new (nothrow) Msr::Register("JIDR", JIDR, false, 0x0ull);
    if (!register_system_reg(reg))
        return false;
    reg = new (nothrow) Msr::Register("FCSEIDR", FCSEIDR, true, 0x0ull, 0x0ull);
    if (!register_system_reg(reg))
        return false;
    reg = new (nothrow) Msr::Register("TCMTR", TCMTR, true, 0x0ull, 0x0ull);
    if (!register_system_reg(reg))
        return false;
    reg = new (nothrow) Msr::Register("TLBTR", TLBTR, true, 0x0ull, 0x0ull);
    if (!register_system_reg(reg))
        return false;
    reg = new (nothrow) Msr::Register("ID_AFR0_EL1", ID_AFR0_EL1, false, 0x0ull);

    return register_system_reg(reg);
}

bool
Msr::Bus::setup_arch_msr(const Msr::Bus::PlatformInfo &info, Vbus::Bus &vbus, Model::GicD &gicd) {
    Msr::Register *reg;

    if (!setup_aarch64_features(info.aa64.id_aa64pfr0_el1, info.aa64.id_aa64pfr1_el1,
                                info.aa64.id_aa64isar0_el1, info.aa64.id_aa64isar1_el1,
                                info.aa64.id_aa64zfr0_el1))
        return false;

    if (!setup_aarch64_memory_model(info.aa64.id_aa64mmfr0_el1, info.aa64.id_aa64mmfr1_el1,
                                    info.aa64.id_aa64mmfr2_el1))
        return false;

    if (!setup_aarch64_setway_flushes(vbus))
        return false;

    if (!setup_aarch64_debug(info.aa64.id_aa64dfr0_el1, info.aa64.id_aa64dfr1_el1))
        return false;

    if (!setup_tvm())
        return false;

    if (!setup_aarch64_ras())
        return false;

    if (!setup_aarch64_pms())
        return false;

    /* ID */
    reg = new (nothrow) Msr::Register("AIDR_EL1", AIDR_EL1, false, 0x0ULL);
    if (!register_system_reg(reg))
        return false;
    reg = new (nothrow) Msr::Register("REVIDR_EL1", REVIDR_EL1, false, 0x0ULL);
    if (!register_system_reg(reg))
        return false;

    /*
     *  Performance monitoring
     *  Strictly speaking, we shouldn't have to trap these since we don't report PMU
     *  to be implemented. However, Linux running on QEMU will try to read that. It
     *  could be because certain MSR are not trapped properly with QEMU.
     */
    reg = new (nothrow) Msr::Register("PMUSEREN_EL0", PMUSEREN_EL0, true, 0x0ULL);
    if (!register_system_reg(reg))
        return false;

    /* Locking */
    reg = new (nothrow) Msr::Register("OSDLR_EL1", OSDLR_EL1, true, 0x0ULL);
    if (!register_system_reg(reg))
        return false;
    reg = new (nothrow) Msr::Register("OSLAR_EL1", OSLAR_EL1, true, 0x0ULL);
    if (!register_system_reg(reg))
        return false;
    reg = new (nothrow) Msr::Register("OSLSR_EL1", OSLSR_EL1, true, 0x0ULL);
    if (!register_system_reg(reg))
        return false;

    if (gicd.version() == Model::GIC_V3) {
        if (!setup_gic_registers(gicd))
            return false;
    }

    Msr::Info::IdAa64pfr0 aa64pfr0(info.aa64.id_aa64pfr0_el1);

    if (aa64pfr0.get_supported_mode(Msr::Info::IdAa64pfr0::EL1_SHIFT)
            == Msr::Info::IdAa64pfr0::AA64_AA32
        || aa64pfr0.get_supported_mode(Msr::Info::IdAa64pfr0::EL0_SHIFT)
               == Msr::Info::IdAa64pfr0::AA64_AA32) {
        return setup_aarch32_msr(info);
    }

    return true;
}

Vbus::Err
Msr::IccSgi1rEl1::access(Vbus::Access const access, const VcpuCtx *vcpu_ctx, Vbus::Space, mword,
                         uint8, uint64 &value) {
    if (access != Vbus::WRITE)
        return Vbus::Err::ACCESS_ERR;

    _gic->icc_sgi1r_el1(value, vcpu_ctx->vcpu_id);

    return Vbus::Err::OK;
}

void
Msr::Set_way_flush_reg::flush(const VcpuCtx *vctx, const uint8, const uint32) const {
    /*
     * Set/Way flushing instructions cannot and shouldn't be executed by the VMM.
     * Hence, we choose to replace set/way flushing by VA flushing forcing us to flush
     * the whole address space. This is costly but it should only happen when turning on/off
     * caches. The ARM manual specifies that other usages are bad practice/undefined.
     *
     * We enable TVM to catch toggling of the cache by the guest. When the cache is toggled
     * we then proceed to flushing the guest AS. Semantically, that should be what the
     * guest OS wants to achieve.
     */
    if (!Model::Cpu::is_feature_enabled_on_vcpu(Model::Cpu::requested_feature_tvm, vctx->vcpu_id)) {
        INFO("Use of Set/way flush detected. Enable caching bit tracking");
        Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_tvm, vctx->vcpu_id, true);
    }
}

void
Msr::flush_on_cache_toggle(const VcpuCtx *vcpu, uint64 new_value) {
    if (!Model::Cpu::is_feature_enabled_on_vcpu(Model::Cpu::requested_feature_tvm, vcpu->vcpu_id)) {
        // Another requestor needed TVM - no action to take on our side
        return;
    }

    Msr::Info::SctlrEl1 before(vcpu->regs->el1_sctlr()), after(new_value);

    /*
     * This is the counter-part of the Set/Way flushing logic emulation. Every time the
     * cache is toggled, we flush the guest AS. Moreover, if the cache is enabled we stop
     * trapping the virtual memory registers and wait for an eventual new (nothrow) call to Set/way
     * instructions before flushing again.
     *
     * Note that for now, VMI is not interested in that event so we simply don't forward it.
     */

    if (before.cache_enabled() != after.cache_enabled()) {
        INFO("Cache setting toggled - flushing the guest AS. EL1_SCTLR = %#llx",
             vcpu->regs->el1_sctlr());

        Vbus::Bus *bus = Msr::Set_way_flush_reg::get_associated_bus();
        ASSERT(bus != nullptr);
        bus->iter_devices<const VcpuCtx>(Model::SimpleAS::flush_callback, nullptr);
    }
    if (after.cache_enabled()) {
        INFO("Cache enabled - stop TVM trapping");
        Model::Cpu::ctrl_feature_on_vcpu(Model::Cpu::ctrl_feature_tvm, vcpu->vcpu_id, false);
    }
}

Vbus::Err
Msr::SctlrEl1::access(Vbus::Access access, const VcpuCtx *vcpu, Vbus::Space, mword, uint8,
                      uint64 &res) {
    ASSERT(access == Vbus::Access::WRITE); // We only trap writes at the moment

    flush_on_cache_toggle(vcpu, res);
    return Vbus::Err::UPDATE_REGISTER; // Tell the VCPU to update the relevant physical
                                       // register
}
