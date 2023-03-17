/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#include <debug_switches.hpp>
#include <model/cpu.hpp>
#include <model/cpu_affinity.hpp>
#include <model/gic.hpp>
#include <model/vcpu_types.hpp>
#include <platform/algorithm.hpp>
#include <platform/bits.hpp>
#include <platform/log.hpp>

enum {
    GICD_CTLR = 0x0,
    GICD_CTLR_END = 0x3,
    GICD_TYPER = 0x4,
    GICD_TYPER_END = 0x7,
    GICD_IIDR = 0x8,
    GICD_IIDR_END = 0xb,
    GICD_RESERVED_0 = 0xc,
    GICD_RESERVED_0_END = 0xf,
    GICD_STATUSR = 0x10,
    GICD_STATUSR_END = 0x13,
    GICD_RESERVED_1 = 0x14,
    GICD_RESERVED_1_END = 0x3f,
    GICD_RESERVED_2 = 0x40,
    GICD_RESERVED_2_END = 0x7f,
    GICD_IGROUP = 0x80,
    GICD_IGROUP_END = 0xff,
    GICD_ISENABLER = 0x100,
    GICD_ISENABLER_END = 0x17f,
    GICD_ICENABLER = 0x180,
    GICD_ICENABLER_END = 0x1ff,
    GICD_ISPENDR = 0x200,
    GICD_ISPENDR_END = 0x27f,
    GICD_ICPENDR = 0x280,
    GICD_ICPENDR_END = 0x2ff,
    GICD_ISACTIVER = 0x300,
    GICD_ISACTIVER_END = 0x379,
    GICD_ICACTIVER = 0x380,
    GICD_ICACTIVER_END = 0x3ff,
    GICD_IPRIORITYR = 0x400,
    GICD_IPRIORITYR_END = 0x7ff,
    GICD_ITARGETSR0 = 0x800,
    GICD_ITARGETSR0_END = 0x81f,
    GICD_ITARGETSR8 = 0x820,
    GICD_ITARGETSR8_END = 0xbff,
    GICD_ICFGR0 = 0xc00,
    GICD_ICFGR0_END = 0xc03,
    GICD_ICFGR1 = 0xc04,
    GICD_ICFGR1_END = 0xc07,
    GICD_ICFGR = 0xc08,
    GICD_ICFGR_END = 0xcff,
    GICD_RESERVED_3 = 0xd00,
    GICD_RESERVED_3_END = 0xeff,
    GICD_SGIR = 0xf00,
    GICD_SGIR_END = 0xf03,
    GICD_RESERVED_4 = 0xf04,
    GICD_RESERVED_4_END = 0xf0f,
    GICD_CPENDSGIR = 0xf10,
    GICD_CPENDSGIR_END = 0xf1f,
    GICD_SPENDSGIR = 0xf20,
    GICD_SPENDSGIR_END = 0xf2f,
    GICD_RESERVED_19 = 0x0f30,
    GICD_RESERVED_19_END = 0x60ff,
    GICD_IROUTER = 0x6100,
    GICD_IROUTER_END = 0x7fdf,
    GICD_RESERVED_20 = 0x7fe0,
    GICD_RESERVED_20_END = 0xbfff,
    GICD_IMPLDEF_0 = 0xc000,
    GICD_IMPLDEF_0_END = 0xffcf,
    GICD_PIDR4 = 0xffd0,
    GICD_PIDR4_END = 0xffd3,
    GICD_PIDR5 = 0xffd4,
    GICD_PIDR5_END = 0xffd7,
    GICD_PIDR6 = 0xffd8,
    GICD_PIDR6_END = 0xffdb,
    GICD_PIDR7 = 0xffdc,
    GICD_PIDR7_END = 0xffdf,
    GICD_PIDR0 = 0xffe0,
    GICD_PIDR0_END = 0xffe3,
    GICD_PIDR1 = 0xffe4,
    GICD_PIDR1_END = 0xffe7,
    GICD_PIDR2 = 0xffe8,
    GICD_PIDR2_END = 0xffeb,
    GICD_PIDR3 = 0xffec,
    GICD_PIDR3_END = 0xffef,
    GICD_IMPLDEF_X = 0xfff0,
    GICD_IMPLDEF_X_END = 0xffff
};

static constexpr unsigned GICD_SIZE = 0x10000;

class Sgir {
private:
    uint32 const _value;

public:
    explicit Sgir(uint32 const value) : _value(value) {}

    enum {
        FILTER_USE_LIST = 0,
        FILTER_ALL_BUT_ME = 1,
        FILTER_ONLY_ME = 2,
    };

    uint8 sgi() const { return _value & 0xf; }
    uint8 targets() const { return (_value >> 16) & 0xff; }
    uint8 filter() const { return (_value >> 24) & 0x3; }

    bool target(unsigned const cpu) const {
        if (cpu >= sizeof(targets()) * 8)
            return false;
        if (cpu >= Model::GICV2_MAX_CPUS)
            return false;
        return (targets() & (1u << cpu)) != 0u;
    }
};

bool
Model::GicD::read_register(uint64 const offset, uint32 const base_reg, uint32 const base_max, uint8 const bytes,
                           uint64 const value, uint64 &result) {
    if ((bytes == 0u) || (bytes > 8) || (offset + bytes > base_max + 1))
        return false;

    uint64 const base = offset - base_reg;
    uint64 const mask = (bytes >= 8) ? (0ULL - 1) : ((1ULL << (bytes * 8)) - 1);
    result = (value >> (base * 8)) & mask;
    return true;
}

Vbus::Err
Model::GicD::access(Vbus::Access const access, const VcpuCtx *vcpu_ctx, Vbus::Space, mword const offset, uint8 const size,
                    uint64 &value) {

    bool ok = false;

    if (access == Vbus::Access::WRITE)
        ok = mmio_write(vcpu_ctx->vcpu_id, offset, size, value);
    if (access == Vbus::Access::READ)
        ok = mmio_read(vcpu_ctx->vcpu_id, offset, size, value);

    return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
}

bool
Model::GicD::write_ctlr(uint64 offset, uint8 bytes, uint64 value) {
    constexpr uint32 const GICD_GRP0 = 1 << 0;
    constexpr uint32 const GICD_GRP1 = 1 << 1;
    constexpr uint32 const GICD_ARE = 1 << 4;
    constexpr uint32 const ENFORCE_ZERO_V2 = ~(GICD_GRP0 | GICD_GRP1);
    constexpr uint32 const ENFORCE_ZERO = ~(GICD_ARE | GICD_GRP0 | GICD_GRP1);
    RegAccess acc;

    acc.offset = offset;
    acc.base_reg = GICD_CTLR;
    acc.base_max = GICD_CTLR_END;
    acc.bytes = bytes;

    return write_register(acc, value, _ctlr.value, (_version >= 3) ? ENFORCE_ZERO : ENFORCE_ZERO_V2);
}

bool
Model::GicD::write_irouter(Banked &cpu, uint64 offset, uint8 bytes, uint64 value) {
    if (!_ctlr.affinity_routing())
        return true; /* WI */
    if (bytes != 8 || (offset % 8 != 0))
        return true; /* ignore - spec says this is not supported */

    uint64 const irq_id = SPI_BASE + (offset - GICD_IROUTER) / 8;
    if (irq_id < SPI_BASE || irq_id >= configured_irqs())
        return true; /* ignore */

    Irq &irq = irq_object(cpu, irq_id);

    if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
        INFO("GOS requested IRQ %u to be routed to VCPU " FMTu64, irq.id(), value);

    irq.routing.value = value;

    if (irq.pending())
        redirect_spi(irq, ++_vcpu_global_hint);

    return true;
}

bool
Model::GicD::write_sgir(Vcpu_id cpu_id, uint64 value) {
    Sgir const sgir(value & 0xfffffffful);

    switch (sgir.filter()) {
    case Sgir::FILTER_USE_LIST: {
        for (Vcpu_id tcpu = 0; tcpu < sizeof(sgir.targets()) * 8; tcpu++) {
            if (!sgir.target(static_cast<uint32>(tcpu)))
                continue;

            send_sgi(cpu_id, tcpu, sgir.sgi());
        }
        break;
    }
    case Sgir::FILTER_ALL_BUT_ME:
        for (Vcpu_id tcpu = 0; tcpu < Model::GICV2_MAX_CPUS; tcpu++) {
            if (tcpu == cpu_id)
                continue;

            send_sgi(cpu_id, tcpu, sgir.sgi());
        }
        break;
    case Sgir::FILTER_ONLY_ME:
        send_sgi(cpu_id, cpu_id, sgir.sgi());
        break;
    default:
        break;
    }

    return true;
}

bool
Model::GicD::mmio_write_32_or_less(Vcpu_id cpu_id, IrqMmioAccess &acc, uint64 value) {
    Banked &cpu = _local[cpu_id];

    switch (acc.offset) {
    case GICD_CTLR ... GICD_CTLR_END:
        return write_ctlr(acc.offset, acc.bytes, value);
    case GICD_IGROUP ... GICD_IGROUP_END:
        acc.base_abs = GICD_IGROUP;
        return write<bool, &Irq::set_group1>(cpu, acc, value);
    case GICD_ISENABLER ... GICD_ISENABLER_END:
        acc.base_abs = GICD_ISENABLER;
        return write<bool, &Irq::enable>(cpu, acc, value);
    case GICD_ICENABLER ... GICD_ICENABLER_END:
        acc.base_abs = GICD_ICENABLER;
        return write<bool, &Irq::disable>(cpu, acc, value);
    case GICD_ISPENDR ... GICD_ISPENDR_END: {
        uint64 reg = value;
        if (acc.offset == GICD_ISPENDR)
            reg &= ~((1ULL << MAX_SGI) - 1); /* SGIs are WI */
        acc.base_abs = GICD_ISPENDR;
        return mmio_assert<&GicD::assert_pi_sw>(cpu_id, acc, reg);
    }
    case GICD_ICPENDR ... GICD_ICPENDR_END: {
        uint64 reg = value;
        if (acc.offset == GICD_ICPENDR)
            reg &= ~((1ULL << MAX_SGI) - 1); /* SGIs are WI */
        acc.base_abs = GICD_ICPENDR;
        return mmio_assert<&GicD::deassert_pi_sw>(cpu_id, acc, reg);
    }
    case GICD_ISACTIVER ... GICD_ISACTIVER_END:
        acc.base_abs = GICD_ISACTIVER;
        return write<bool, &Irq::activate>(cpu, acc, value);
    case GICD_ICACTIVER ... GICD_ICACTIVER_END:
        acc.base_abs = GICD_ICACTIVER;
        return write<bool, &Irq::deactivate>(cpu, acc, value);
    case GICD_IPRIORITYR ... GICD_IPRIORITYR_END:
        acc.irq_per_bytes = 1;
        acc.base_abs = GICD_IPRIORITYR;
        return write<uint8, &Irq::prio>(cpu, acc, value);
    case GICD_ITARGETSR8 ... GICD_ITARGETSR8_END:
        acc.irq_per_bytes = 1;
        acc.base_abs = GICD_ITARGETSR8;
        acc.configure_access(AccessType::SPI);
        return change_target(cpu, acc, value);
    case GICD_ICFGR1 ... GICD_ICFGR1_END:
        acc.base_abs = GICD_ICFGR1;
        acc.irq_per_bytes = 4;
        acc.configure_access(AccessType::PPI);
        return write<uint8, &Irq::set_encoded_edge>(cpu, acc, value);
    case GICD_ICFGR ... GICD_ICFGR_END:
        acc.base_abs = GICD_ICFGR;
        acc.irq_per_bytes = 4;
        acc.configure_access(AccessType::SPI);
        return write<uint8, &Irq::set_encoded_edge>(cpu, acc, value);
    case GICD_SGIR ... GICD_SGIR_END:
        return write_sgir(cpu_id, value);
    case GICD_CPENDSGIR ... GICD_CPENDSGIR_END: {
        if (_ctlr.affinity_routing())
            return true; /* WI */

        acc.irq_per_bytes = 1;
        acc.base_abs = GICD_CPENDSGIR;
        acc.configure_access(AccessType::SGI);
        return mmio_assert_sgi<&GicD::deassert_sgi>(cpu_id, acc, value);
    }
    case GICD_SPENDSGIR ... GICD_SPENDSGIR_END: {
        if (_ctlr.affinity_routing())
            return true; /* WI */

        acc.irq_per_bytes = 1;
        acc.base_abs = GICD_SPENDSGIR;
        acc.configure_access(AccessType::SGI);
        return mmio_assert_sgi<&GicD::assert_sgi>(cpu_id, acc, value);
    }
    case GICD_TYPER ... GICD_TYPER_END:             /* RO */
    case GICD_IIDR ... GICD_IIDR_END:               /* RO */
    case GICD_RESERVED_0 ... GICD_RESERVED_0_END:   /* WI */
    case GICD_STATUSR ... GICD_STATUSR_END:         /* optional - not implemented */
    case GICD_RESERVED_1 ... GICD_RESERVED_1_END:   /* WI */
    case GICD_RESERVED_2 ... GICD_RESERVED_2_END:   /* WI */
    case GICD_RESERVED_3 ... GICD_RESERVED_3_END:   /* WI */
    case GICD_RESERVED_4 ... GICD_RESERVED_4_END:   /* WI */
    case GICD_ITARGETSR0 ... GICD_ITARGETSR0_END:   /* WI */
    case GICD_RESERVED_19 ... GICD_RESERVED_19_END: /* WI */
    case GICD_RESERVED_20 ... GICD_RESERVED_20_END: /* WI */
    case GICD_ICFGR0 ... GICD_ICFGR0_END:           /* WI */
    case GICD_IMPLDEF_0 ... GICD_IMPLDEF_0_END:     /* impl. defined */
    case GICD_PIDR4 ... GICD_PIDR4_END:             /* RO */
    case GICD_PIDR5 ... GICD_PIDR5_END:             /* RO */
    case GICD_PIDR6 ... GICD_PIDR6_END:             /* RO */
    case GICD_PIDR7 ... GICD_PIDR7_END:             /* RO */
    case GICD_PIDR0 ... GICD_PIDR0_END:             /* RO */
    case GICD_PIDR1 ... GICD_PIDR1_END:             /* RO */
    case GICD_PIDR2 ... GICD_PIDR2_END:             /* RO */
    case GICD_PIDR3 ... GICD_PIDR3_END:             /* RO */
    case GICD_IMPLDEF_X ... GICD_IMPLDEF_X_END:     /* impl. defined */
        return true;
    }

    WARN("GICD: ignored write @ %#llx", acc.offset);
    return true;
}

bool
Model::GicD::mmio_write(Vcpu_id const cpu_id, uint64 const offset, uint8 const bytes, uint64 const value) {

    if (offset >= GICD_SIZE || bytes > ACCESS_SIZE_32 * 2 || cpu_id >= _num_vcpus)
        return false;

    Banked &cpu = _local[cpu_id];

    switch (offset) {
    case GICD_IROUTER ... GICD_IROUTER_END:
        return write_irouter(cpu, offset, bytes, value);
    }

    if (bytes > ACCESS_SIZE_32)
        return false;

    IrqMmioAccess acc{.base_abs = 0, // Filled by the logic below
                      .irq_base = 0,
                      .irq_max = MAX_SGI + MAX_SPI + MAX_PPI,
                      .offset = offset,
                      .bytes = bytes,
                      .irq_per_bytes = 8, // Work with a bitfield by default
                      .configured_irqs = configured_irqs()};
    acc.configure_access(GicD::AccessType::ALL); // default

    return mmio_write_32_or_less(cpu_id, acc, value);
}

bool
Model::GicD::read_pending(Banked &cpu, IrqMmioAccess &acc, uint32 base_offset, uint64 &value) const {
    if (_ctlr.affinity_routing()) {
        value = 0;
        return true; /* RAZ */
    }

    acc.irq_per_bytes = 1;
    acc.irq_max = MAX_SGI;
    acc.base_abs = base_offset;
    return read<bool, &Irq::pending>(cpu, acc, value);
}

bool
Model::GicD::mmio_read_32_or_less(Vcpu_id cpu_id, IrqMmioAccess &acc, uint64 &value) const {
    Banked &cpu = _local[cpu_id];

    switch (acc.offset) {
    case GICD_CTLR ... GICD_CTLR_END:
        return read_register(acc.offset, GICD_CTLR, GICD_CTLR_END, acc.bytes, _ctlr.value, value);
    case GICD_TYPER ... GICD_TYPER_END:
        return read_register(acc.offset, GICD_TYPER, GICD_TYPER_END, acc.bytes, get_typer(), value);
    case GICD_IIDR ... GICD_IIDR_END:
        return read_register(acc.offset, GICD_IIDR, GICD_IIDR_END, acc.bytes, 0x123, value);
    case GICD_IGROUP ... GICD_IGROUP_END:
        acc.base_abs = GICD_IGROUP;
        return read<bool, &Irq::group1>(cpu, acc, value);
    case GICD_ISENABLER ... GICD_ISENABLER_END:
        acc.base_abs = GICD_ISENABLER;
        return read<bool, &Irq::enabled>(cpu, acc, value);
    case GICD_ICENABLER ... GICD_ICENABLER_END:
        acc.base_abs = GICD_ICENABLER;
        return read<bool, &Irq::enabled>(cpu, acc, value);
    case GICD_ISPENDR ... GICD_ISPENDR_END:
        acc.base_abs = GICD_ISPENDR;
        return read<bool, &Irq::pending>(cpu, acc, value);
    case GICD_ICPENDR ... GICD_ICPENDR_END:
        acc.base_abs = GICD_ICPENDR;
        return read<bool, &Irq::pending>(cpu, acc, value);
    case GICD_ISACTIVER ... GICD_ISACTIVER_END:
        acc.base_abs = GICD_ISACTIVER;
        return read<bool, &Irq::active>(cpu, acc, value);
    case GICD_ICACTIVER ... GICD_ICACTIVER_END:
        acc.base_abs = GICD_ICACTIVER;
        return read<bool, &Irq::active>(cpu, acc, value);
    case GICD_IPRIORITYR ... GICD_IPRIORITYR_END:
        acc.irq_per_bytes = 1;
        acc.base_abs = GICD_IPRIORITYR;
        return read<uint8, &Irq::prio>(cpu, acc, value);
    case GICD_ITARGETSR0 ... GICD_ITARGETSR0_END:
        acc.irq_per_bytes = 1;
        acc.base_abs = GICD_ITARGETSR0;
        acc.configure_access(AccessType::PRIVATE_ONLY);
        return read<uint8, &Irq::target>(cpu, acc, value);
    case GICD_ITARGETSR8 ... GICD_ITARGETSR8_END:
        acc.irq_per_bytes = 1;
        acc.base_abs = GICD_ITARGETSR8;
        acc.configure_access(AccessType::SPI);
        return read<uint8, &Irq::target>(cpu, acc, value);
    case GICD_ICFGR0 ... GICD_ICFGR0_END:
        acc.irq_per_bytes = 4;
        acc.base_abs = GICD_ICFGR0;
        acc.configure_access(AccessType::SGI);
        return read<uint8, &Irq::edge_encoded>(cpu, acc, value);
    case GICD_ICFGR1 ... GICD_ICFGR1_END:
        acc.irq_per_bytes = 4;
        acc.base_abs = GICD_ICFGR1;
        acc.configure_access(AccessType::PPI);
        return read<uint8, &Irq::edge_encoded>(cpu, acc, value);
    case GICD_ICFGR ... GICD_ICFGR_END:
        acc.irq_per_bytes = 4;
        acc.base_abs = GICD_ICFGR;
        acc.configure_access(AccessType::SPI);
        return read<uint8, &Irq::edge_encoded>(cpu, acc, value);
    case GICD_PIDR4 ... GICD_PIDR4_END:
        value = 0x44;
        return true;
    case GICD_PIDR5 ... GICD_PIDR5_END:
    case GICD_PIDR6 ... GICD_PIDR6_END:
    case GICD_PIDR7 ... GICD_PIDR7_END:
    case GICD_PIDR3 ... GICD_PIDR3_END:
        value = 0x0;
        return true;
    case GICD_PIDR0 ... GICD_PIDR0_END:
        value = 0x92;
        return true;
    case GICD_PIDR1 ... GICD_PIDR1_END:
        value = 0xb4;
        return true;
    case GICD_PIDR2 ... GICD_PIDR2_END:
        value = (static_cast<uint64>(_version) << 4) | 0xb;
        return true;
    case GICD_CPENDSGIR ... GICD_CPENDSGIR_END:
        return read_pending(cpu, acc, GICD_CPENDSGIR, value);
    case GICD_SPENDSGIR ... GICD_SPENDSGIR_END:
        return read_pending(cpu, acc, GICD_SPENDSGIR, value);
    default:
        /* GICD_IMPLDEF_0 ... GICD_IMPLDEF_0_END: impl. defined
         * GICD_IMPLDEF_X ... GICD_IMPLDEF_X_END: impl. defined
         * GICD_SGIR ... GICD_SGIR_END: write - only
         * GICD_RESERVED_0 ... GICD_RESERVED_0_END: RAZ
         * GICD_STATUSR ... GICD_STATUSR_END: optional - not implemented
         * GICD_RESERVED_1 ... GICD_RESERVED_1_END: RAZ
         * GICD_RESERVED_2 ... GICD_RESERVED_2_END: RAZ
         * GICD_RESERVED_3 ... GICD_RESERVED_3_END: RAZ
         * GICD_RESERVED_4 ... GICD_RESERVED_4_END: RAZ
         * GICD_RESERVED_19 ... GICD_RESERVED_19_END: RAZ
         * GICD_RESERVED_20 ... GICD_RESERVED_20_END: RAZ */
        value = 0;
        WARN("GICD: ignored read @ %#llx", acc.offset);
        return true;
    }
    __UNREACHED__;
}

bool
Model::GicD::mmio_read(Vcpu_id const cpu_id, uint64 const offset, uint8 const bytes, uint64 &value) const {

    if (offset >= GICD_SIZE || (bytes > ACCESS_SIZE_32 * 2) || cpu_id >= _num_vcpus)
        return false;

    Banked const &cpu = _local[cpu_id];

    switch (offset) {
    case GICD_IROUTER ... GICD_IROUTER_END: {
        uint64 const irq_id = SPI_BASE + (offset - GICD_IROUTER) / 8;
        value = 0ULL;
        if (bytes != 8 || (offset % 8 != 0))
            return true; /* ignore - spec says this is not supported */
        if (irq_id < SPI_BASE || irq_id >= configured_irqs())
            return true; /* ignore */
        if (!_ctlr.affinity_routing())
            return true; /* RAZ */

        Irq const &irq = irq_object(cpu, irq_id);
        value = irq.routing.value;

        return true;
    }
    }

    if (bytes > ACCESS_SIZE_32)
        return false;

    IrqMmioAccess acc{.base_abs = 0, // Filled by the logic below
                      .irq_base = 0,
                      .irq_max = MAX_SGI + MAX_SPI + MAX_PPI,
                      .offset = offset,
                      .bytes = bytes,
                      .irq_per_bytes = 8, // Work with a bitfield by default
                      .configured_irqs = configured_irqs()};
    acc.configure_access(GicD::AccessType::ALL); // default

    return mmio_read_32_or_less(cpu_id, acc, value);
}

bool
Model::GicD::config_irq(Vcpu_id const cpu_id, uint32 const irq_id, bool const hw, uint16 const pintid, bool edge) {
    if (irq_id >= configured_irqs())
        return false;
    if (cpu_id >= _num_vcpus)
        return false;

    Banked &cpu = _local[cpu_id];
    Irq &irq = irq_object(cpu, irq_id);
    irq.configure_hw(hw, pintid, edge);

    return true;
}

bool
Model::GicD::assert_ppi(Vcpu_id const cpu_id, uint32 const irq_id) {
    if (cpu_id >= _num_vcpus)
        return false;
    if (irq_id >= SPI_BASE)
        return false;

    Banked &cpu = _local[cpu_id];
    Irq &irq = irq_object(cpu, irq_id);

    if (!irq.hw_edge())
        irq.assert_line();

    return assert_pi(cpu_id, irq);
}

void
Model::GicD::deassert_line(Vcpu_id const cpu_id, uint32 const irq_id) {
    if (cpu_id >= _num_vcpus)
        return;
    if (irq_id >= SPI_BASE)
        return;

    Banked &cpu = _local[cpu_id];
    Irq &irq = irq_object(cpu, irq_id);

    irq.deassert_line();

    /*
     * Check if the interrupt is configured as level. If it is and the guest
     * didn't set the pending bit in software, then we have to deassert (clear
     * the pending bit) the interrupt.
     */
    if (!irq.sw_edge() && !irq.sw_asserted())
        deassert_pi(cpu_id, irq);
}

void
Model::GicD::deassert_line_ppi(Vcpu_id const cpu_id, uint32 const irq_id) {
    deassert_line(cpu_id, irq_id);
}

void
Model::GicD::deassert_global_line(uint32 const irq_id) {
    deassert_line(0, irq_id);
}

bool
Model::GicD::has_irq_in_injection(Vcpu_id const cpu_id) {
    Banked &cpu = _local[cpu_id];
    size_t irq_id = cpu.in_injection_irqs.first_set(0, configured_irqs() - 1);

    return irq_id != Bitset<MAX_IRQ>::NOT_FOUND;
}

Model::GicD::Irq *
Model::GicD::highest_irq(Vcpu_id const cpu_id, bool redirect_irq) {
    Model::GicD::Irq *r = nullptr;
    size_t irq_id = 0;
    Banked &cpu = _local[cpu_id];
    const Local_Irq_controller *gic_r = _local[cpu_id].notify->local_irq_ctlr();

    do {
        irq_id = cpu.pending_irqs.first_set(irq_id, configured_irqs() - 1);
        if (irq_id == Bitset<MAX_IRQ>::NOT_FOUND)
            break;

        Irq &irq = irq_object(cpu, irq_id);
        IrqInjectionInfoUpdate cur = irq.injection_info.read();

        if (((irq.group0() && _ctlr.group0_enabled()) || (irq.group1() && _ctlr.group1_enabled())) && cur.is_targeting_cpu(cpu_id)
            && cur.pending() && irq.enabled() && !cpu.in_injection_irqs.is_set(irq_id) && vcpu_can_receive_irq(gic_r)) {

            if (r == nullptr || irq.prio() > r->prio())
                r = &irq;
        } else if (redirect_irq && (irq_id >= SPI_BASE) && !vcpu_can_receive_irq(gic_r)) {
            // or (irq.group0() && !vmcr.group0_enabled() && _ctlr.affinity_routing())
            // or (irq.group1() && !vmcr.group1_enabled() && _ctlr.affinity_routing()))) {
            /*
             * If this interface is not capable of receiving the IRQ anymore,
             * in the GICv3 world (affinity_routing enabled), we have to release
             * it so that another interface can handle it.
             */
            redirect_spi(irq, cpu_id + 1); // kick it to the next one (modulo will apply)
        }

        irq_id++;
    } while (irq_id < configured_irqs());

    return r;
}

bool
Model::GicD::any_irq_active(Vcpu_id cpu_id) {
    Banked &cpu = _local[cpu_id];

    for (uint32 i = 0; i < configured_irqs(); i++) {
        Irq &irq = irq_object(cpu, i);

        if (irq.active())
            return true;
    }

    return false;
}

bool
Model::GicD::pending_irq(Vcpu_id const cpu_id, Lr &lr, uint8 min_priority) {
    ASSERT(cpu_id < _num_vcpus);
    Irq *irq = highest_irq(cpu_id, true);
    if ((irq == nullptr) || (min_priority < irq->prio()))
        return false;
    ASSERT(irq->id() < configured_irqs());

    IrqInjectionInfoUpdate desired, cur;
    uint8 sender_id;

    do {
        cur = irq->injection_info.read();

        if (!cur.pending() || !cur.is_targeting_cpu(cpu_id)) {
            lr = Lr(0);
            return true;
        }

        sender_id = cur.get_pending_sender_id();

        if (irq->id() >= PPI_BASE || _ctlr.affinity_routing())
            ASSERT(sender_id == 0);

        desired = cur;
        desired.set_injected(sender_id);
    } while (!irq->injection_info.cas(cur, desired));

    Banked &cpu = _local[cpu_id];
    IrqState state = IrqState::PENDING;

    cpu.in_injection_irqs.atomic_set(irq->id());
    cpu.pending_irqs.atomic_clr(irq->id());

    /*
     * The spec says that a hypervisor should never set the active and pending state
     * for a HW originated interrupt. So, even if the interrupt was forcefully set to
     * both state, we have to ignore that.
     */
    if (irq->active() && !irq->hw()) {
        state = IrqState::ACTIVE_PENDING;
    }

    if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
        INFO("Injecting IRQ %u on VCPU " FMTu64, irq->id(), cpu_id);

    if (__UNLIKELY__(Debug::current_level == Debug::FULL && irq->id() < MAX_SGI)) {
        if (is_affinity_routing_enabled())
            INFO("Injecting SGI %u on VCPU " FMTu64, irq->id(), cpu_id)
        else
            INFO("Injecting SGI %u from %u on VCPU " FMTu64, irq->id(), sender_id, cpu_id);
    }

    lr = Lr(state, *irq, irq->id(), sender_id);

    return true;
}

void
Model::GicD::update_inj_status_inactive(Vcpu_id const cpu_id, uint32 irq_id) {
    Banked &cpu = _local[cpu_id];
    Irq &irq = irq_object(cpu, irq_id);

    // Done injecting
    if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
        INFO("IRQ %u handled by the guest on VCPU " FMTu64, irq_id, cpu_id);

    if (Stats::enabled())
        irq.num_acked++;

    IrqInjectionInfoUpdate desired, cur;

    irq.deactivate();

    do {
        cur = irq.injection_info.read();
        uint8 sender_id = cur.get_injected_sender_id();

        if (sender_id == IrqInjectionInfoUpdate::NO_INJECTION or !cur.is_injected(sender_id) or !cur.is_targeting_cpu(cpu_id))
            break;

        desired = cur;
        desired.unset_injected(sender_id);
        desired.unset_pending(sender_id);
    } while (!irq.injection_info.cas(cur, desired));

    if (irq.pending())
        cpu.pending_irqs.atomic_set(irq.id());
}

void
Model::GicD::update_inj_status_active_or_pending(Vcpu_id const cpu_id, IrqState state, uint32 irq_id, bool in_injection) {
    Banked &cpu = _local[cpu_id];
    Irq &irq = irq_object(cpu, irq_id);

    if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
        INFO("IRQ %u came back, not yet injected on VCPU " FMTu64, irq_id, cpu_id);

    if (state == IrqState::PENDING) {
        irq.deactivate();
    } else {
        irq.activate();
    }

    IrqInjectionInfoUpdate desired, cur;
    do {
        cur = irq.injection_info.read();
        uint8 sender_id = cur.get_injected_sender_id();

        if (sender_id == IrqInjectionInfoUpdate::NO_INJECTION or !cur.is_injected(sender_id) or !cur.is_targeting_cpu(cpu_id))
            break;

        desired = cur;
        if (!in_injection)
            desired.unset_injected(sender_id);
        if (state == IrqState::ACTIVE)
            desired.unset_pending(sender_id);
    } while (!irq.injection_info.cas(cur, desired));

    if (irq.pending())
        cpu.pending_irqs.atomic_set(irq.id());
}

void
Model::GicD::update_inj_status(Vcpu_id const cpu_id, uint32 irq_id, IrqState state, bool in_injection) {
    ASSERT(cpu_id < _num_vcpus);
    ASSERT(irq_id < configured_irqs());
    Banked &cpu = _local[cpu_id];
    Irq &irq = irq_object(cpu, irq_id);

    if (!in_injection) {
        ASSERT(state == PENDING or state == INACTIVE);
        cpu.in_injection_irqs.atomic_clr(irq.id());
    }

    switch (state) {
    case IrqState::INACTIVE:
        update_inj_status_inactive(cpu_id, irq_id);
        return;
    case IrqState::ACTIVE:
    case IrqState::ACTIVE_PENDING:
    case IrqState::PENDING:
        update_inj_status_active_or_pending(cpu_id, state, irq_id, in_injection);
    }
}

bool
Model::GicD::notify_target(Irq &irq, const IrqTarget &target) {
    if (__UNLIKELY__(!target.is_valid())) {
        return false;
    }

    if (target.is_targeting_a_set()) {
        for (uint16 i = 0; i < std::min<uint16>(_num_vcpus, Model::GICV2_MAX_CPUS); i++) {
            if (!target.is_cpu_targeted(i))
                continue;

            Banked *target_cpu = &_local[i];
            const Local_Irq_controller *gic_r = target_cpu->notify->local_irq_ctlr();

            target_cpu->pending_irqs.atomic_set(irq.id());

            // Avoid recalling a VCPU that has silenced IRQs
            if (__LIKELY__(vcpu_can_receive_irq(gic_r)))
                target_cpu->notify->notify_interrupt_pending();
        }
    } else {
        Banked *target_cpu = &_local[target.target()];
        const Local_Irq_controller *gic_r = target_cpu->notify->local_irq_ctlr();

        target_cpu->pending_irqs.atomic_set(irq.id());

        if (__LIKELY__(vcpu_can_receive_irq(gic_r)))
            target_cpu->notify->notify_interrupt_pending();
    }

    return true;
}

bool
Model::GicD::redirect_spi(Irq &irq, Vcpu_id vcpu_hint_start) {
    ASSERT(irq.id() >= SPI_BASE);

    IrqTarget target = route_spi(irq, vcpu_hint_start);

    if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
        INFO("SPI %u re-routed to VCPU" FMTx32, irq.id(), target.raw());

    IrqInjectionInfoUpdate desired(0), cur;

    do {
        cur = irq.injection_info.read();
        if (!cur.pending() || cur.is_injected())
            return false; // Prevent injecting the IRQ twice, we just want to reroute here

        desired.set_target_cpu(target);
        desired.set_pending();
    } while (!irq.injection_info.cas(cur, desired));

    return notify_target(irq, target);
}

bool
Model::GicD::assert_pi_sw(Vcpu_id cpu_id, Irq &irq) {
    ASSERT(irq.id() >= PPI_BASE || _ctlr.affinity_routing());
    irq.assert_sw();
    return assert_pi(cpu_id, irq);
}

bool
Model::GicD::assert_pi(Vcpu_id cpu_id, Irq &irq) {
    IrqTarget target;

    ASSERT(irq.id() >= PPI_BASE || _ctlr.affinity_routing());

    if (irq.id() >= SPI_BASE) {
        target = route_spi(irq, ++_vcpu_global_hint);

        if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
            INFO("SPI %u routed to VCPU" FMTx32, irq.id(), target.raw());
    } else {
        target = IrqTarget(IrqTarget::CPU_ID, cpu_id);
    }

    IrqInjectionInfoUpdate update(0);

    update.set_target_cpu(target);
    update.set_pending();
    irq.injection_info.set(update);

    if (Stats::enabled())
        irq.num_asserted++;

    return notify_target(irq, target);
}

bool
Model::GicD::assert_sgi(Vcpu_id sender, Vcpu_id target, Irq &irq) {
    ASSERT(irq.id() < MAX_SGI);

    if (_ctlr.affinity_routing())
        return assert_pi(target, irq);

    if (Model::GICV2_MAX_CPUS <= sender)
        return false;

    IrqInjectionInfoUpdate desired, cur;

    do {
        cur = irq.injection_info.read();
        desired = cur;
        desired.set_target_cpu(IrqTarget(IrqTarget::CPU_ID, target));
        desired.unset_injected(static_cast<uint8>(sender));
        desired.set_pending(static_cast<uint8>(sender));
    } while (!irq.injection_info.cas(cur, desired));

    if (Stats::enabled())
        irq.num_asserted++;

    if (__UNLIKELY__(Debug::current_level == Debug::FULL))
        INFO("SGI %u sent from " FMTx64 " to " FMTx64, irq.id(), sender, target);

    return notify_target(irq, IrqTarget(IrqTarget::CPU_ID, target));
}

bool
Model::GicD::deassert_pi_sw(Vcpu_id vcpu_id, Irq &irq) {
    ASSERT(irq.id() >= PPI_BASE || _ctlr.affinity_routing());

    irq.deassert_sw();

    return deassert_pi(vcpu_id, irq);
}

bool
Model::GicD::deassert_pi(Vcpu_id, Irq &irq) {
    ASSERT(irq.id() >= PPI_BASE || _ctlr.affinity_routing());

    if (irq.hw()) {
        INFO("Hardware interrupts behave as level-triggered. Pending kept on for %u", irq.id());
        return true;
    }

    if (Stats::enabled() and irq.pending())
        irq.num_acked++;

    IrqInjectionInfoUpdate update(0);
    irq.injection_info.set(update);

    return true;
}

bool
Model::GicD::deassert_sgi(Vcpu_id sender, Vcpu_id target, Irq &irq) {
    if (_ctlr.affinity_routing())
        return deassert_pi(target, irq);

    if (Model::GICV2_MAX_CPUS <= sender)
        return false;

    IrqInjectionInfoUpdate desired, cur;

    do {
        cur = irq.injection_info.read();
        desired = cur;
        desired.unset_injected(static_cast<uint8>(sender));
        desired.unset_pending(static_cast<uint8>(sender));
    } while (!irq.injection_info.cas(cur, desired));

    return true;
}

Model::GicD::IrqTarget
Model::GicD::route_spi_no_affinity(Model::GicD::Irq &irq) {
    constexpr uint16 TARGET_MODE_MAX_CPUS = 8U;
    IrqTarget res(IrqTarget::CPU_SET, 0);

    for (Vcpu_id i = 0; i < std::min<uint16>(_num_vcpus, TARGET_MODE_MAX_CPUS); i++) {
        if (_local[i].notify == nullptr)
            continue;

        if ((irq.target() & (1u << i)) != 0u)
            res.add_target_to_set(i);
    }

    return res;
}

Model::GicD::IrqTarget
Model::GicD::route_spi(Model::GicD::Irq &irq, Vcpu_id vcpu_hint_start) {
    // XXX: once an interface is enabled, we need to reroute those interrupts....

    if (!_ctlr.affinity_routing())
        return route_spi_no_affinity(irq);

    if (irq.routing.any()) {
        vcpu_hint_start %= _num_vcpus;

        // Ingore group0/1 enabled here - we will do the routing
        // if ((irq.group0() && !_ctlr.group0_enabled()) or (irq.group1() &&
        // !_ctlr.group1_enabled()))
        //     return IrqTarget(); // Empty target;

        uint64 cpus_tried = 0;
        do {
            Cpu_irq_interface *const cpu = _local[vcpu_hint_start].notify;
            ASSERT(cpu);

            const Local_Irq_controller *gicr = cpu->local_irq_ctlr();
            if (gicr->can_receive_irq())
                return IrqTarget(IrqTarget::CPU_ID, vcpu_hint_start);

            cpus_tried++;
            vcpu_hint_start++;
            vcpu_hint_start %= _num_vcpus;
        } while (cpus_tried != _num_vcpus);

        /*
         * Nobody was nice enough to accept that interrupt... It is possible that all
         * VCPUs are sleeping or disabled. We park the IRQ in the queue of the current
         * hint as a default. That VCPU will that kick that IRQ again down the road as
         * appropriate.
         */
        return IrqTarget(IrqTarget::CPU_ID, vcpu_hint_start);
    } else {
        const CpuAffinity cpu_aff
            = CpuAffinity(static_cast<uint32>(irq.routing.aff3() << 24) | static_cast<uint32>(irq.routing.aff2() << 16)
                          | static_cast<uint32>(irq.routing.aff1() << 8) | static_cast<uint32>(irq.routing.aff0()));
        const CpuCluster *cluster = cpu_affinity_to_cluster(cpu_aff);
        if (__UNLIKELY__(cluster == nullptr)) {
            WARN("Cluster with affinity %u does not exist", cpu_aff.affinity());
            return IrqTarget(); // Empty target
        }

        Vcpu_id vcpu_id = cluster->vcpu_id(cpu_aff.aff0());

        if (vcpu_id >= _num_vcpus)
            return IrqTarget(); // Empty target
        if (_local[vcpu_id].notify != nullptr)
            return IrqTarget(IrqTarget::CPU_ID, vcpu_id);
    }

    return IrqTarget(); // Empty target
}

bool
Model::GicD::assert_global_line(uint32 const irq_id) {
    if (irq_id >= configured_irqs())
        return false;
    if (irq_id < SPI_BASE)
        return false;

    Irq &irq = _spi[irq_id - SPI_BASE];

    if (!irq.hw_edge())
        irq.assert_line();

    return assert_pi(0, irq); // cpu_id zero, this won't matter for a SPI
}

bool
Model::GicD::config_spi(uint32 const vintid, bool const hw, uint16 const pintid, bool edge) {
    return config_irq(0, vintid, hw, pintid, edge);
}

void
Model::GicD::enable_cpu(Cpu_irq_interface *cpu, Vcpu_id const cpu_id) {
    ASSERT(cpu_id < _num_vcpus);
    /* for now only once a cpu can register */
    ASSERT(_local[cpu_id].notify == nullptr);

    _local[cpu_id].notify = cpu;
}

void
Model::GicD::disable_cpu(Vcpu_id const cpu_id) {
    ASSERT(cpu_id < _num_vcpus);
    ASSERT(_local[cpu_id].notify != nullptr);

    _local[cpu_id].notify = nullptr;
}

class IccSgi1rEl1 {
private:
    uint64 const _value;

public:
    explicit IccSgi1rEl1(uint64 const value) : _value(value) {}

    static constexpr uint8 MAX_CPU_ID_IN_TARGET_LIST = 16;

    uint16 targets() const { return _value & 0xffff; }
    uint8 intid() const { return (_value >> 24) & 0xf; }
    uint8 irm() const { return (_value >> 40) & 0x1; }

    uint32 cluster_affinity() const {
        return (static_cast<uint32>(aff1()) << 8u) | (static_cast<uint32>(aff2()) << 16u) | (static_cast<uint32>(aff3()) << 24u);
    }
    uint8 aff1() const { return (_value >> 16) & 0xff; }
    uint8 aff2() const { return (_value >> 32) & 0xff; }
    uint8 aff3() const { return (_value >> 48) & 0xff; }

    bool target(unsigned const cpu) const {
        if (cpu >= sizeof(targets()) * 8)
            return false;
        if (cpu >= MAX_CPU_ID_IN_TARGET_LIST)
            return false;
        return (targets() & (1u << cpu)) != 0u;
    }
};

void
Model::GicD::icc_sgi1r_el1(uint64 const value, Vcpu_id const self) {
    IccSgi1rEl1 const sysreg(value);

    if (sysreg.intid() >= PPI_BASE)
        return;

    if (sysreg.irm() != 0u) {
        for (Vcpu_id tcpu = 0; tcpu < _num_vcpus; tcpu++) {
            if (tcpu == self)
                continue;

            send_sgi(self, tcpu, sysreg.intid());
        }
    } else {
        const CpuCluster *cluster = cpu_affinity_to_cluster(CpuAffinity(sysreg.cluster_affinity()));

        if (__UNLIKELY__(cluster == nullptr)) {
            WARN("Cluster with affinity %u does not exist", sysreg.cluster_affinity());
            return;
        }

        for (uint8 tcpu = 0; tcpu < IccSgi1rEl1::MAX_CPU_ID_IN_TARGET_LIST; tcpu++) {
            if (!sysreg.target(static_cast<uint32>(tcpu)))
                continue;

            Vcpu_id vid = cluster->vcpu_id(tcpu);
            if (vid == INVALID_VCPU_ID)
                continue;

            send_sgi(self, vid, sysreg.intid());
        }
    }
}

void
Model::GicD::send_sgi(Vcpu_id const from, Vcpu_id const target, uint32 const sgi_id) {
    ASSERT(sgi_id < MAX_SGI);
    ASSERT(target < _num_vcpus);

    Banked &target_cpu = _local[target];
    Irq &irq = target_cpu.sgi[sgi_id];

    assert_sgi(from, target, irq);
}

void
Model::GicD::reset_status_bitfields_on_vcpu(uint16 vcpu_idx) {
    for (uint32 i = 0; i < configured_irqs(); i++) {
        Irq &irq = irq_object(_local[vcpu_idx], i);

        if (irq.hw()) {
            if (_local[vcpu_idx].in_injection_irqs.is_set(i)) {
                _local[vcpu_idx].pending_irqs.atomic_set(i);
            }
        } else {
            _local[vcpu_idx].pending_irqs.atomic_clr(i);
        }
    }

    _local[vcpu_idx].in_injection_irqs.reset();
}

void
Model::GicD::reset(const VcpuCtx *) {
    for (uint16 cpu = 0; cpu < _num_vcpus; cpu++) {
        for (uint8 i = 0; i < MAX_SGI; i++) {
            _local[cpu].sgi[i].reset(static_cast<uint8>(1u << cpu));
            /*
             * The spec says: Whether SGIs are permanently enabled, or can be enabled and disabled
             * by writes to the GICD_ISENABLERn and GICD_ICENABLERn, is IMPLEMENTATION DEFINED.
             * Therefore, it is safer to start with SGIs as enabled. Guests may assume they are
             * already on.
             */
            _local[cpu].sgi[i].enable();
        }
        for (uint8 i = 0; i < MAX_PPI; i++)
            _local[cpu].ppi[i].reset(static_cast<uint8>(1u << cpu));

        reset_status_bitfields_on_vcpu(cpu);
    }

    for (uint32 spi = 0; spi < configured_spis(); spi++) {
        _spi[spi].reset(1);
    }
    _ctlr.value = 0;
}
