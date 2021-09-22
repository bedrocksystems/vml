/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/gic.hpp>

/*
 * Note that this implementation does not support LPI at the moment.
 * LPIs are optional in the ARM spec and would make this code more complicated.
 * The list of register below does not include register that are implementation defined when
 * LPIs are not supported.
 */

enum {
    GICR_CTLR = 0x0,
    GICR_CTLR_END = 0x3,
    GICR_IIDR = 0x4,
    GICR_IIDR_END = 0x7,
    GICR_TYPER = 0x8,
    GICR_TYPER_END = 0xf,
    GICR_STATUSR = 0x10, // Optional, we don't implement it
    GICR_STATUSR_END = 0x13,
    GICR_WAKER = 0x14,
    GICR_WAKER_END = 0x17,
    GICR_PROPBASER = 0x70, // LPIs are not supported
    GICR_PROPBASER_END = 0x77,
    GICR_PENDBASER = 0x78, // LPIs are not supported
    GICR_PENDBASER_END = 0x7f,
    GICR_PIDR2 = 0xffe8,
    GICR_PIDR2_END = 0xffeb,
    GICR_IGROUP0 = 0x10080,
    GICR_IGROUP0_END = 0x10083,
    GICR_ISENABLER0 = 0x10100,
    GICR_ISENABLER0_END = 0x10103,
    GICR_ICENABLER0 = 0x10180,
    GICR_ICENABLER0_END = 0x10183,
    GICR_ISPENDR0 = 0x10200,
    GICR_ISPENDR0_END = 0x10203,
    GICR_ICPENDR0 = 0x10280,
    GICR_ICPENDR0_END = 0x10283,
    GICR_ISACTIVER0 = 0x10300,
    GICR_ISACTIVER0_END = 0x10303,
    GICR_ICACTIVER0 = 0x10380,
    GICR_ICACTIVER0_END = 0x10383,
    GICR_IPRIORITYR0 = 0x10400,
    GICR_IPRIORITYR0_END = 0x1041f,
    GICR_ICFGR0 = 0x10c00,
    GICR_ICFGR0_END = 0x10c03,
    GICR_ICFGR1 = 0x10c04,
    GICR_ICFGR1_END = 0x10c07,
    GICR_IGRPMODR0 = 0x10d00, // Secure state only, we can ignore writes
    GICR_IGRPMODR0_END = 0x10d03,
    GICR_NSACR = 0x10e00, // Secure state only, we can ignore writes
    GICR_NSACR_END = 0x10e03,
};

constexpr uint16 GICR_IIDR_IMPLEMENTER = 0x43b;

enum { GICR_SIZE = 0x20000 };

bool
Model::GicR::can_receive_irq() const {
    return !_waker.sleeping();
}

Vbus::Err
Model::GicR::access(Vbus::Access const access, const VcpuCtx *, Vbus::Space, mword const offset,
                    uint8 const size, uint64 &value) {

    bool ok = false;

    if (access == Vbus::Access::WRITE)
        ok = mmio_write(offset, size, value);
    if (access == Vbus::Access::READ)
        ok = mmio_read(offset, size, value);

    return ok ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
}

bool
Model::GicR::mmio_write(uint64 const offset, uint8 const bytes, uint64 const value) {
    if (offset >= GICR_SIZE)
        return false;
    if (bytes > ACCESS_SIZE_32)
        return false;

    GicD &gic = *_gic_d;
    ASSERT(_vcpu_id < gic._num_vcpus);
    GicD::Banked &cpu = gic._local[_vcpu_id];

    GicD::IrqMmioAccess acc{.base_abs = 0, // Filled by the logic below
                            .irq_base = 0,
                            .irq_max = MAX_SGI + MAX_PPI,
                            .offset = offset,
                            .bytes = bytes,
                            .irq_per_bytes = 8}; // Work with a bitfield by default

    switch (offset) {
    case GICR_CTLR ... GICR_CTLR_END:
    case GICR_PROPBASER ... GICR_PROPBASER_END:
    case GICR_PENDBASER ... GICR_PENDBASER_END:
    case GICR_IGRPMODR0 ... GICR_IGRPMODR0_END:
    case GICR_NSACR ... GICR_NSACR_END:
        /* RAZ/WI */
        return true;
    case GICR_WAKER ... GICR_WAKER_END: {
        Waker w;
        uint32 new_w = 0;
        GicD::RegAccess reg_acc{
            .offset = offset, .base_reg = GICR_WAKER, .base_max = GICR_WAKER_END, .bytes = bytes};

        w.value = static_cast<uint32>(value);
        new_w = (uint32(w.sleeping()) << Waker::CHILDREN_ASLEEP_BIT)
                | (uint32(w.sleeping()) << Waker::SLEEP_BIT);
        return gic.write_register<uint32>(reg_acc, new_w, _waker.value, Waker::RESV_ZERO);
    }
    case GICR_IGROUP0 ... GICR_IGROUP0_END:
        acc.base_abs = GICR_IGROUP0;
        return gic.write<bool, &GicD::Irq::set_group1>(cpu, acc, value);
    case GICR_ISENABLER0 ... GICR_ISENABLER0_END:
        acc.base_abs = GICR_ISENABLER0;
        return gic.write<bool, &GicD::Irq::enable>(cpu, acc, value);
    case GICR_ICENABLER0 ... GICR_ICENABLER0_END:
        acc.base_abs = GICR_ICENABLER0;
        return gic.write<bool, &GicD::Irq::disable>(cpu, acc, value);
    case GICR_ISACTIVER0 ... GICR_ISACTIVER0_END:
        acc.base_abs = GICR_ISACTIVER0;
        return gic.write<bool, &GicD::Irq::activate>(cpu, acc, value);
    case GICR_ICACTIVER0 ... GICR_ICACTIVER0_END:
        acc.base_abs = GICR_ICACTIVER0;
        return gic.write<bool, &GicD::Irq::deactivate>(cpu, acc, value);
    case GICR_IPRIORITYR0 ... GICR_IPRIORITYR0_END:
        acc.base_abs = GICR_IPRIORITYR0;
        acc.irq_per_bytes = 1;
        return gic.write<uint8, &GicD::Irq::prio>(cpu, acc, value);
    case GICR_ISPENDR0 ... GICR_ISPENDR0_END:
        if (!gic.is_affinity_routing_enabled())
            return false;
        acc.base_abs = GICR_ISPENDR0;
        return gic.mmio_assert<&GicD::assert_pi_sw>(_vcpu_id, acc, value);
    case GICR_ICPENDR0 ... GICR_ICPENDR0_END:
        if (!gic.is_affinity_routing_enabled())
            return false;
        acc.base_abs = GICR_ICPENDR0;
        return gic.mmio_assert<&GicD::deassert_pi_sw>(_vcpu_id, acc, value);
    case GICR_ICFGR0 ... GICR_ICFGR0_END:
        acc.base_abs = GICR_ICFGR0;
        acc.irq_max = MAX_SGI;
        acc.irq_per_bytes = 4;
        return gic.write<uint8, &GicD::Irq::set_encoded_edge>(cpu, acc, value);
    case GICR_ICFGR1 ... GICR_ICFGR1_END:
        acc.base_abs = GICR_ICFGR1;
        acc.irq_base = MAX_SGI;
        acc.irq_max = MAX_PPI;
        acc.irq_per_bytes = 4;
        return gic.write<uint8, &GicD::Irq::set_encoded_edge>(cpu, acc, value);
    }

    WARN("GICR: ignored write @ %#llx", offset);
    return true;
}

bool
Model::GicR::mmio_read(uint64 const offset, uint8 const bytes, uint64 &value) const {
    if (offset >= GICR_SIZE)
        return false;

    GicD &gic = *_gic_d;
    ASSERT(_vcpu_id < gic._num_vcpus);
    GicD::Banked const &cpu = gic._local[_vcpu_id];

    switch (offset) {
    case GICR_TYPER ... GICR_TYPER_END: {
        uint64 ret = uint64(_aff.aff3()) << 56 | uint64(_aff.aff2()) << 48
                     | uint64(_aff.aff1()) << 40 | uint64(_aff.aff0()) << 32;
        ret |= uint64(_aff.aff1()) << 16 | uint64(_aff.aff0()) << 8; /* processor id */
        ret |= (_last ? 1ull : 0ull) << 4;                           /* last re-distributor */
        return gic.read_register(offset, GICR_TYPER, GICR_TYPER_END, bytes, ret, value);
    }
    case GICR_WAKER ... GICR_WAKER_END: {
        return gic.read_register(offset, GICR_WAKER, GICR_WAKER_END, bytes, _waker.value, value);
    }
    }

    if (bytes > ACCESS_SIZE_32)
        return false;

    GicD::IrqMmioAccess acc{.base_abs = 0, // Filled by the logic below
                            .irq_base = 0,
                            .irq_max = MAX_SGI + MAX_PPI,
                            .offset = offset,
                            .bytes = bytes,
                            .irq_per_bytes = 8}; // Work with a bitfield by default

    switch (offset) {
    case GICR_CTLR ... GICR_CTLR_END:
        return gic.read_register(offset, GICR_CTLR, GICR_CTLR_END, bytes, 0ull, value);
    case GICR_IIDR ... GICR_IIDR_END:
        return gic.read_register(offset, GICR_IIDR, GICR_IIDR_END, bytes, GICR_IIDR_IMPLEMENTER,
                                 value);
    case GICR_PIDR2 ... GICR_PIDR2_END:
        return gic.read_register(offset, GICR_PIDR2, GICR_PIDR2_END, bytes, 3ull << 4, value);
    case GICR_PROPBASER ... GICR_PROPBASER_END:
    case GICR_PENDBASER ... GICR_PENDBASER_END:
    case GICR_IGRPMODR0 ... GICR_IGRPMODR0_END:
    case GICR_NSACR ... GICR_NSACR_END:
        /* RAZ/WI */
        value = 0ULL;
        return true;
    case GICR_ISENABLER0 ... GICR_ISENABLER0_END:
        acc.base_abs = GICR_ISENABLER0;
        return gic.read<bool, &GicD::Irq::enabled>(cpu, acc, value);
    case GICR_ICENABLER0 ... GICR_ICENABLER0_END:
        acc.base_abs = GICR_ICENABLER0;
        return gic.read<bool, &GicD::Irq::enabled>(cpu, acc, value);
    case GICR_ISACTIVER0 ... GICR_ISACTIVER0_END:
        acc.base_abs = GICR_ISACTIVER0;
        return gic.read<bool, &GicD::Irq::active>(cpu, acc, value);
    case GICR_ICACTIVER0 ... GICR_ICACTIVER0_END:
        acc.base_abs = GICR_ICACTIVER0;
        return gic.read<bool, &GicD::Irq::active>(cpu, acc, value);
    case GICR_ISPENDR0 ... GICR_ISPENDR0_END:
        acc.base_abs = GICR_ISPENDR0;
        return gic.read<bool, &GicD::Irq::pending>(cpu, acc, value);
    case GICR_ICPENDR0 ... GICR_ICPENDR0_END:
        acc.base_abs = GICR_ICPENDR0;
        return gic.read<bool, &GicD::Irq::pending>(cpu, acc, value);
    case GICR_IGROUP0 ... GICR_IGROUP0_END:
        acc.base_abs = GICR_IGROUP0;
        return gic.read<bool, &GicD::Irq::group1>(cpu, acc, value);
    case GICR_ICFGR0 ... GICR_ICFGR0_END:
        acc.base_abs = GICR_ICFGR0;
        acc.irq_max = MAX_SGI;
        acc.irq_per_bytes = 4;
        return gic.read<uint8, &GicD::Irq::edge_encoded>(cpu, acc, value);
    case GICR_ICFGR1 ... GICR_ICFGR1_END:
        acc.base_abs = GICR_ICFGR1;
        acc.irq_base = MAX_SGI;
        acc.irq_max = MAX_PPI;
        acc.irq_per_bytes = 4;
        return gic.read<uint8, &GicD::Irq::edge_encoded>(cpu, acc, value);
    case GICR_IPRIORITYR0 ... GICR_IPRIORITYR0_END:
        acc.base_abs = GICR_IPRIORITYR0;
        acc.irq_per_bytes = 1;
        return gic.read<uint8, &GicD::Irq::prio>(cpu, acc, value);
    }

    WARN("GICR: ignored read @ %#llx", offset);
    return true;
}
