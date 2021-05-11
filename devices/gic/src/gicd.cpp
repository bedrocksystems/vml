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
    bool group1() const { return (_value >> 15) & 0x1; }

    bool target(unsigned const cpu) const {
        if (cpu >= sizeof(targets()) * 8)
            return false;
        if (cpu >= Model::GICV2_MAX_CPUS)
            return false;
        return targets() & (1u << cpu);
    }
};

bool
Model::GicD::read_register(uint64 const offset, uint32 const base_reg, uint32 const base_max,
                           uint8 const bytes, uint64 const value, uint64 &result) const {
    if (!bytes || (bytes > 8) || (offset + bytes > base_max + 1))
        return false;

    uint64 const base = offset - base_reg;
    uint64 const mask = (bytes >= 8) ? (0ULL - 1) : ((1ULL << (bytes * 8)) - 1);
    result = (value >> (base * 8)) & mask;
    return true;
}

Vbus::Err
Model::GicD::access(Vbus::Access const access, const VcpuCtx *vcpu_ctx, Vbus::Space,
                    mword const offset, uint8 const size, uint64 &value) {

    bool ok = false;

    if (access == Vbus::Access::WRITE)
        ok = mmio_write(vcpu_ctx->vcpu_id, offset, size, value);
    if (access == Vbus::Access::READ)
        ok = mmio_read(vcpu_ctx->vcpu_id, offset, size, value);

    return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
}

bool
Model::GicD::mmio_write(Vcpu_id const cpu_id, uint64 const offset, uint8 const bytes,
                        uint64 const value) {

    if (offset >= GICD_SIZE || bytes > ACCESS_SIZE_32 * 2 || cpu_id >= _num_vcpus)
        return false;

    Banked &cpu = _local[cpu_id];

    switch (offset) {
    case GICD_IROUTER ... GICD_IROUTER_END:
        if (!_ctlr.affinity_routing())
            return true; /* WI */
        if (bytes != 8 || (offset % 8 != 0))
            return true; /* ignore - spec says this is not supported */

        uint64 const irq_id = MAX_SGI + MAX_PPI + (offset - GICD_IROUTER) / 8;
        if (irq_id < MAX_SGI + MAX_PPI || irq_id >= MAX_IRQ)
            return true; /* ignore */

        Irq &irq = irq_object(cpu, irq_id);

        if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
            INFO("GOS requested IRQ %u to be routed to VCPU " FMTu64, irq.id(), value);

        irq._routing.value = value;

        return true;
    }

    if (bytes > ACCESS_SIZE_32)
        return false;

    IrqMmioAccess acc{.base_abs = 0, // Filled by the logic below
                      .irq_base = 0,
                      .irq_max = MAX_SGI + MAX_SPI + MAX_PPI,
                      .offset = offset,
                      .bytes = bytes,
                      .irq_per_bytes = 8}; // Work with a bitfield by default

    switch (offset) {
    case GICD_CTLR ... GICD_CTLR_END: {
        constexpr uint32 const GICD_GRP0 = 1 << 0;
        constexpr uint32 const GICD_GRP1 = 1 << 1;
        constexpr uint32 const GICD_ARE = 1 << 4;
        constexpr uint32 const ENFORCE_ZERO_V2 = ~(GICD_GRP0 | GICD_GRP1);
        constexpr uint32 const ENFORCE_ZERO = ~(GICD_ARE | GICD_GRP0 | GICD_GRP1);
        return write_register(offset, GICD_CTLR, GICD_CTLR_END, bytes, value, _ctlr.value,
                              (_version >= 3) ? ENFORCE_ZERO : ENFORCE_ZERO_V2);
    }
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
        if (offset == GICD_ISPENDR)
            reg &= ~((1ULL << MAX_SGI) - 1); /* SGIs are WI */
        acc.base_abs = GICD_ISPENDR;
        return mmio_assert<&GicD::assert_pi>(cpu_id, acc, reg);
    }
    case GICD_ICPENDR ... GICD_ICPENDR_END: {
        uint64 reg = value;
        if (offset == GICD_ICPENDR)
            reg &= ~((1ULL << MAX_SGI) - 1); /* SGIs are WI */
        acc.base_abs = GICD_ICPENDR;
        return mmio_assert<&GicD::deassert_pi>(cpu_id, acc, reg);
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
        acc.irq_base = MAX_SGI + MAX_PPI;
        acc.irq_max = MAX_SPI;
        acc.base_abs = GICD_ITARGETSR8;
        return change_target(cpu, acc, value);
    case GICD_ICFGR ... GICD_ICFGR_END:
        acc.base_abs = GICD_ICFGR;
        acc.irq_per_bytes = 4;
        return write<uint8, &Irq::set_encoded_edge>(cpu, acc, value);
    case GICD_SGIR ... GICD_SGIR_END: {
        Sgir const sgir(value & 0xfffffffful);

        switch (sgir.filter()) {
        case Sgir::FILTER_USE_LIST: {
            for (Vcpu_id tcpu = 0; tcpu < sizeof(sgir.targets()) * 8; tcpu++) {
                if (!sgir.target(static_cast<uint32>(tcpu)))
                    continue;

                send_sgi(cpu_id, tcpu, sgir.sgi(), true, sgir.group1());
            }
            break;
        }
        case Sgir::FILTER_ALL_BUT_ME:
            for (Vcpu_id tcpu = 0; tcpu < Model::GICV2_MAX_CPUS; tcpu++) {
                if (tcpu == cpu_id)
                    continue;

                send_sgi(cpu_id, tcpu, sgir.sgi(), true, sgir.group1());
            }
            break;
        case Sgir::FILTER_ONLY_ME:
            send_sgi(cpu_id, cpu_id, sgir.sgi(), true, sgir.group1());
            break;
        default:
            break;
        }

        return true;
    }
    case GICD_CPENDSGIR ... GICD_CPENDSGIR_END: {
        if (_ctlr.affinity_routing())
            return true; /* WI */

        acc.irq_per_bytes = 1;
        acc.base_abs = GICD_CPENDSGIR;
        acc.irq_max = MAX_SGI;
        return mmio_assert_sgi<&GicD::deassert_sgi>(cpu_id, acc, value);
    }
    case GICD_SPENDSGIR ... GICD_SPENDSGIR_END: {
        if (_ctlr.affinity_routing())
            return true; /* WI */

        acc.irq_per_bytes = 1;
        acc.base_abs = GICD_SPENDSGIR;
        acc.irq_max = MAX_SGI;
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
    case GICD_ICFGR1 ... GICD_ICFGR1_END:           /* WI */
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

    return false;
}

bool
Model::GicD::mmio_read(Vcpu_id const cpu_id, uint64 const offset, uint8 const bytes,
                       uint64 &value) const {

    if (offset >= GICD_SIZE || (bytes > ACCESS_SIZE_32 * 2) || cpu_id >= _num_vcpus)
        return false;

    Banked const &cpu = _local[cpu_id];

    switch (offset) {
    case GICD_IROUTER ... GICD_IROUTER_END: {
        uint64 const irq_id = MAX_SGI + MAX_PPI + (offset - GICD_IROUTER) / 8;
        value = 0ULL;
        if (bytes != 8 || (offset % 8 != 0))
            return true; /* ignore - spec says this is not supported */
        if (irq_id < MAX_SGI + MAX_PPI || irq_id >= MAX_IRQ)
            return true; /* ignore */
        if (!_ctlr.affinity_routing())
            return true; /* RAZ */

        Irq const &irq = irq_object(cpu, irq_id);
        value = irq._routing.value;

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
                      .irq_per_bytes = 8}; // Work with a bitfield by default

    switch (offset) {
    case GICD_CTLR ... GICD_CTLR_END:
        return read_register(offset, GICD_CTLR, GICD_CTLR_END, bytes, _ctlr.value, value);
    case GICD_TYPER ... GICD_TYPER_END: {
        uint64 typer = 31ULL /* ITLinesNumber */ | (uint64(_num_vcpus - 1) << 5) /* CPU count */
                       | (9ULL << 19)                                            /* id bits */
                       | (1ULL << 24); /* Aff3 supported */
        return read_register(offset, GICD_TYPER, GICD_TYPER_END, bytes, typer, value);
    }
    case GICD_IIDR ... GICD_IIDR_END:
        return read_register(offset, GICD_IIDR, GICD_IIDR_END, bytes, 0x123, value);
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
        acc.irq_base = 0;
        acc.irq_max = MAX_SGI + MAX_PPI;
        acc.base_abs = GICD_ITARGETSR0;
        return read<uint8, &Irq::target>(cpu, acc, value);
    case GICD_ITARGETSR8 ... GICD_ITARGETSR8_END:
        acc.irq_per_bytes = 1;
        acc.irq_base = MAX_SGI + MAX_PPI;
        acc.irq_max = MAX_SPI;
        acc.base_abs = GICD_ITARGETSR8;
        return read<uint8, &Irq::target>(cpu, acc, value);
    case GICD_ICFGR0 ... GICD_ICFGR0_END:
        acc.irq_per_bytes = 4;
        acc.irq_max = MAX_SGI;
        acc.base_abs = GICD_ICFGR0;
        return read<uint8, &Irq::edge_encoded>(cpu, acc, value);
    case GICD_ICFGR1 ... GICD_ICFGR1_END:
        acc.irq_per_bytes = 4;
        acc.irq_base = MAX_SGI + MAX_PPI;
        acc.irq_max = MAX_PPI;
        acc.base_abs = GICD_ICFGR1;
        return read<uint8, &Irq::edge_encoded>(cpu, acc, value);
    case GICD_ICFGR ... GICD_ICFGR_END:
        acc.irq_per_bytes = 4;
        acc.base_abs = GICD_ICFGR;
        return read<uint8, &Irq::edge_encoded>(cpu, acc, value);
    case GICD_PIDR4 ... GICD_PIDR4_END:
        value = 0x44;
        return true;
    case GICD_PIDR5 ... GICD_PIDR5_END:
    case GICD_PIDR6 ... GICD_PIDR6_END:
    case GICD_PIDR7 ... GICD_PIDR7_END:
        value = 0x0;
        return true;
    case GICD_PIDR0 ... GICD_PIDR0_END:
        value = 0x92;
        return true;
    case GICD_PIDR1 ... GICD_PIDR1_END:
        value = 0xb4;
        return true;
    case GICD_PIDR2 ... GICD_PIDR2_END:
        value = (uint64(_version) << 4) | 0xb;
        return true;
    case GICD_PIDR3 ... GICD_PIDR3_END:
        value = 0x0;
        return true;
    case GICD_IMPLDEF_0 ... GICD_IMPLDEF_0_END: /* impl. defined */
    case GICD_IMPLDEF_X ... GICD_IMPLDEF_X_END: /* impl. defined */
    case GICD_SGIR ... GICD_SGIR_END:           /* write - only */
        return true;
    case GICD_CPENDSGIR ... GICD_CPENDSGIR_END:
        if (_ctlr.affinity_routing()) {
            value = 0;
            return true; /* RAZ */
        }

        acc.irq_per_bytes = 1;
        acc.irq_max = MAX_SGI;
        acc.base_abs = GICD_CPENDSGIR;
        return read<bool, &Irq::pending>(cpu, acc, value);
    case GICD_SPENDSGIR ... GICD_SPENDSGIR_END:
        if (_ctlr.affinity_routing()) {
            value = 0;
            return true; /* RAZ */
        }
        acc.irq_per_bytes = 1;
        acc.irq_max = MAX_SGI;
        acc.base_abs = GICD_SPENDSGIR;
        return read<bool, &Irq::pending>(cpu, acc, value);
    case GICD_RESERVED_0 ... GICD_RESERVED_0_END:   /* RAZ */
    case GICD_STATUSR ... GICD_STATUSR_END:         /* optional - not implemented */
    case GICD_RESERVED_1 ... GICD_RESERVED_1_END:   /* RAZ */
    case GICD_RESERVED_2 ... GICD_RESERVED_2_END:   /* RAZ */
    case GICD_RESERVED_3 ... GICD_RESERVED_3_END:   /* RAZ */
    case GICD_RESERVED_4 ... GICD_RESERVED_4_END:   /* RAZ */
    case GICD_RESERVED_19 ... GICD_RESERVED_19_END: /* RAZ */
    case GICD_RESERVED_20 ... GICD_RESERVED_20_END: /* RAZ */
        value = 0;
        return true;
    }

    return false;
}

bool
Model::GicD::config_irq(Vcpu_id const cpu_id, uint32 const irq_id, bool const hw,
                        uint16 const pintid, bool edge) {
    if (irq_id >= MAX_IRQ)
        return false;
    if (cpu_id >= _num_vcpus)
        return false;

    Banked &cpu = _local[cpu_id];
    Irq &irq = irq_object(cpu, irq_id);

    irq._hw = hw;
    irq._pintid = pintid;
    irq.set_hw_edge(edge);

    return true;
}

bool
Model::GicD::assert_ppi(Vcpu_id const cpu_id, uint32 const irq_id) {
    if (cpu_id >= _num_vcpus)
        return false;
    if (irq_id >= MAX_SGI + MAX_PPI)
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
    if (irq_id >= MAX_SGI + MAX_PPI)
        return;

    Banked &cpu = _local[cpu_id];
    Irq &irq = irq_object(cpu, irq_id);

    irq.deassert_line();
}

void
Model::GicD::deassert_line_ppi(Vcpu_id const cpu_id, uint32 const irq_id) {
    deassert_line(cpu_id, irq_id);
}

void
Model::GicD::deassert_line_spi(uint32 const irq_id) {
    deassert_line(0, irq_id);
}

Model::GicD::Irq *
Model::GicD::highest_irq(Vcpu_id const cpu_id) {
    Model::GicD::Irq *r = nullptr;
    size_t irq_id = 0;
    Banked &cpu = _local[cpu_id];
    const Local_Irq_controller *gic_r = _local[cpu_id]._notify->local_irq_ctlr();

    do {
        irq_id = cpu._pending_irqs.first_set(irq_id, MAX_IRQ - 1);
        if (irq_id == Bitset<MAX_IRQ>::NOT_FOUND)
            break;

        Irq &irq = irq_object(cpu, irq_id);

        if ((irq_id >= MAX_PPI + MAX_SGI) && !vcpu_can_receive_irq(gic_r)) {
            /*
             * If this interface is not capable of receiving the IRQ anymore,
             * in the GICv3 world (affinity_routing enabled), we have to release
             * it so that another interface can handle it.
             */
            redirect_spi(irq);
        }

        if (((irq.group0() && _ctlr.group0_enabled()) || (irq.group1() && _ctlr.group1_enabled()))
            && irq.enabled() && !cpu._in_injection_irqs.is_set(irq_id)) {

            if (r == nullptr || irq.prio() > r->prio())
                r = &irq;
        }
        irq_id++;
    } while (irq_id < MAX_IRQ);

    return r;
}

bool
Model::GicD::any_irq_active(Vcpu_id cpu_id) {
    Banked &cpu = _local[cpu_id];

    for (uint32 i = 0; i < MAX_IRQ; i++) {
        Irq &irq = irq_object(cpu, i);

        if (irq.active())
            return true;
    }

    return false;
}

bool
Model::GicD::pending_irq(Vcpu_id const cpu_id, Lr &lr, uint8 min_priority) {
    ASSERT(cpu_id < _num_vcpus);

    Irq *irq = highest_irq(cpu_id);
    if (!irq || (min_priority < irq->prio()))
        return false;
    ASSERT(irq->id() < MAX_IRQ);

    IrqInjectionInfoUpdate desired, cur;
    uint8 sender_id;

    do {
        cur = irq->injection_info.read();

        if (!cur.pending() || !cur.is_targeting_cpu(cpu_id)) {
            lr = Lr(0);
            return true;
        }

        sender_id = cur.get_pending_sender_id();

        if (irq->id() >= MAX_SGI || _ctlr.affinity_routing())
            ASSERT(sender_id == 0);

        desired = cur;
        desired.set_injected(sender_id);
    } while (!irq->injection_info.cas(cur, desired));

    Banked &cpu = _local[cpu_id];
    IrqState state = IrqState::PENDING;

    cpu._in_injection_irqs.atomic_set(irq->id());
    cpu._pending_irqs.atomic_clr(irq->id());

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
Model::GicD::update_inj_status(Vcpu_id const cpu_id, uint32 irq_id, IrqState state) {
    ASSERT(cpu_id < _num_vcpus);
    ASSERT(irq_id < MAX_IRQ);

    Banked &cpu = _local[cpu_id];
    Irq &irq = irq_object(cpu, irq_id);

    cpu._in_injection_irqs.atomic_clr(irq.id());

    switch (state) {
    case IrqState::INACTIVE: {
        // Done injecting
        if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
            INFO("IRQ %u handled by the guest on VCPU " FMTu64, irq_id, cpu_id);

        IrqInjectionInfoUpdate desired, cur;

        irq.deactivate();

        do {
            cur = irq.injection_info.read();
            uint8 sender_id = cur.get_injected_sender_id();

            if (!cur.is_injected(sender_id) || !cur.is_targeting_cpu(cpu_id))
                return;

            desired = cur;
            desired.unset_injected(sender_id);
            desired.unset_pending(sender_id);
        } while (!irq.injection_info.cas(cur, desired));

        if (desired.pending())
            cpu._pending_irqs.atomic_set(irq.id());

        return;
    }
    case IrqState::ACTIVE:
    case IrqState::ACTIVE_PENDING:
    case IrqState::PENDING: {
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

            if (!cur.is_injected(sender_id) || !cur.is_targeting_cpu(cpu_id))
                return;

            desired = cur;
            desired.unset_injected(sender_id);
            if (state == IrqState::ACTIVE)
                desired.unset_pending(sender_id);
        } while (!irq.injection_info.cas(cur, desired));

        if (desired.pending())
            cpu._pending_irqs.atomic_set(irq.id());
    }
    }
}

bool
Model::GicD::notify_target(Irq &irq, const IrqTarget &target) {
    if (__UNLIKELY__(!target.is_valid())) {
        return false;
    }

    if (target.is_target_set()) {
        for (uint16 i = 0; i < min<uint16>(_num_vcpus, Model::GICV2_MAX_CPUS); i++) {
            Banked *target_cpu = &_local[i];
            const Local_Irq_controller *gic_r = target_cpu->_notify->local_irq_ctlr();

            target_cpu->_pending_irqs.atomic_set(irq.id());

            // Avoid recalling a VCPU that has silenced IRQs
            if (__LIKELY__(vcpu_can_receive_irq(gic_r)))
                target_cpu->_notify->interrupt_pending();
        }
    } else {
        Banked *target_cpu = &_local[target.target()];
        const Local_Irq_controller *gic_r = target_cpu->_notify->local_irq_ctlr();

        target_cpu->_pending_irqs.atomic_set(irq.id());

        if (__LIKELY__(vcpu_can_receive_irq(gic_r)))
            target_cpu->_notify->interrupt_pending();
    }

    return true;
}

bool
Model::GicD::redirect_spi(Irq &irq) {
    ASSERT(irq.id() >= MAX_PPI + MAX_SGI);

    IrqTarget target = route_spi(irq);

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
Model::GicD::assert_pi(Vcpu_id cpu_id, Irq &irq) {
    IrqTarget target;

    ASSERT(irq.id() >= MAX_SGI || _ctlr.affinity_routing());

    if (irq.id() > MAX_PPI + MAX_SGI) {
        target = route_spi(irq);

        if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
            INFO("SPI %u routed to VCPU" FMTx32, irq.id(), target.raw());
    } else {
        target = IrqTarget(IrqTarget::CPU_ID, cpu_id);
    }

    IrqInjectionInfoUpdate update(0);

    update.set_target_cpu(target);
    update.set_pending();
    irq.injection_info.set(update);

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

    if (__UNLIKELY__(Debug::current_level == Debug::FULL))
        INFO("SGI %u sent from " FMTx64 " to " FMTx64, irq.id(), sender, target);

    return notify_target(irq, IrqTarget(IrqTarget::CPU_ID, target));
}

bool
Model::GicD::deassert_pi(Vcpu_id, Irq &irq) {
    ASSERT(irq.id() >= MAX_SGI || _ctlr.affinity_routing());

    if (irq._hw) {
        INFO("Hardware interrupts behave as level-triggered. Pending kept on for %u", irq.id());
        return true;
    }

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
Model::GicD::route_spi(Model::GicD::Irq &irq) {
    if (!_ctlr.affinity_routing()) {
        constexpr uint16 TARGET_MODE_MAX_CPUS = 8U;
        IrqTarget res(IrqTarget::CPU_SET, 0);

        for (Vcpu_id i = 0; i < min<uint16>(_num_vcpus, TARGET_MODE_MAX_CPUS); i++) {
            if (!_local[i]._notify)
                continue;
            if (irq.group0() && !_ctlr.group0_enabled())
                continue;
            if (irq.group1() && !_ctlr.group1_enabled())
                continue;

            if ((irq.target() & (1u << i)))
                res.add_target_to_set(i);
        }

        return res;
    }

    if (irq._routing.any()) {
        for (Vcpu_id i = 0; i < _num_vcpus; i++) {
            Cpu_irq_interface *const cpu = _local[i]._notify;
            ASSERT(cpu);

            if (irq.group0() && !_ctlr.group0_enabled())
                continue;
            if (irq.group1() && !_ctlr.group1_enabled())
                continue;

            const Local_Irq_controller *gicr = cpu->local_irq_ctlr();

            if (gicr->can_receive_irq())
                return IrqTarget(IrqTarget::CPU_ID, i);
        }
    } else {
        uint32 const cpu_id = static_cast<uint32>(irq._routing.aff3() << 24)
                              | static_cast<uint32>(irq._routing.aff2() << 16)
                              | static_cast<uint32>(irq._routing.aff1() << 8)
                              | static_cast<uint32>(irq._routing.aff0());
        if (cpu_id >= _num_vcpus)
            return IrqTarget(); // Empty target
        if (_local[cpu_id]._notify)
            return IrqTarget(IrqTarget::CPU_ID, cpu_id);
    }

    return IrqTarget(); // Empty target
}

bool
Model::GicD::assert_spi(uint32 const irq_id) {
    if (irq_id >= MAX_IRQ)
        return false;
    if (irq_id < MAX_SGI + MAX_PPI)
        return false;

    Irq &irq = _spi[irq_id - MAX_SGI - MAX_PPI];

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
    ASSERT(_local[cpu_id]._notify == nullptr);

    _local[cpu_id]._notify = cpu;
}

class IccSgi1rEl1 {
private:
    uint64 const _value;

public:
    explicit IccSgi1rEl1(uint64 const value) : _value(value) {}

    static constexpr uint8 MAX_CPU_ID_IN_TARGET_LIST = 16;

    uint16 targets() const { return _value & 0xffff; }
    uint8 intid() const { return (_value >> 24) & 0xf; }
    uint8 irm() const { return (_value >> 30) & 0x1; }

    uint32 cluster_affinity() const {
        return (uint32(aff1()) << 8u) | (uint32(aff2()) << 16u) | (uint32(aff3()) << 24u);
    }
    uint8 aff1() const { return (_value >> 16) & 0xff; }
    uint8 aff2() const { return (_value >> 32) & 0xff; }
    uint8 aff3() const { return (_value >> 48) & 0xff; }

    bool target(unsigned const cpu) const {
        if (cpu >= sizeof(targets()) * 8)
            return false;
        if (cpu >= MAX_CPU_ID_IN_TARGET_LIST)
            return false;
        return targets() & (1u << cpu);
    }
};

void
Model::GicD::icc_sgi1r_el1(uint64 const value, Vcpu_id const self) {
    IccSgi1rEl1 const sysreg(value);

    if (sysreg.intid() >= MAX_SGI)
        return;

    if (sysreg.irm()) {
        for (Vcpu_id tcpu = 0; tcpu < _num_vcpus; tcpu++) {
            if (tcpu == self)
                continue;

            send_sgi(self, tcpu, sysreg.intid(), false, true);
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

            send_sgi(self, vid, sysreg.intid(), false, true);
        }
    }
}

void
Model::GicD::send_sgi(Vcpu_id const self, Vcpu_id const cpu, unsigned const sgi,
                      bool const both_groups, bool const sgir_group1) {
    ASSERT(sgi < MAX_SGI);
    ASSERT(cpu < _num_vcpus);

    Banked &target_cpu = _local[cpu];
    Irq &irq = target_cpu._sgi[sgi];

    if (both_groups) {
        /* both groups permitted, they must just match, e.g. 0 == 0 or 1 == 1 */
        if (sgir_group1 != irq.group1())
            return;
    } else {
        /* the specified sgir_group must exactly match */
        if (sgir_group1 && irq.group0())
            return;
        if (!sgir_group1 && irq.group1())
            return;
    }

    assert_sgi(self, cpu, irq);
}

void
Model::GicD::reset(const VcpuCtx *) {
    for (uint16 cpu = 0; cpu < _num_vcpus; cpu++) {
        for (uint8 i = 0; i < MAX_SGI; i++) {
            _local[cpu]._sgi[i].reset(uint8(1u << cpu));
        }
        for (uint8 i = 0; i < MAX_PPI; i++)
            _local[cpu]._ppi[i].reset(uint8(1u << cpu));

        for (uint32 i = 0; i < MAX_IRQ; i++) {
            Irq &irq = irq_object(_local[cpu], i);

            if (irq.hw()) {
                if (_local[cpu]._in_injection_irqs.is_set(i)) {
                    _local[cpu]._pending_irqs.atomic_set(i);
                }
            } else {
                _local[cpu]._pending_irqs.atomic_clr(i);
            }
        }

        _local[cpu]._in_injection_irqs.reset();
    }

    for (uint32 spi = 0; spi < MAX_SPI; spi++) {
        _spi[spi].reset(1);
    }
    _ctlr.value = 0;
}
