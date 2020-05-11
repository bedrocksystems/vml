/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <bitset.hpp>
#include <errno.hpp>
#include <model/gic.hpp>
#include <nova/types.hpp>
#include <range.hpp>
#include <types.hpp>

namespace Fdt {
    class Tree;
}

namespace Passthru {
    struct Irq_translation;
    class Device;
}

struct Passthru::Irq_translation {
    mword phys_intr;
    uint32 virt_intr;
};

/*
 * For now a passthrough device will have the following resources:
 * - N interrupt with N associated semaphores. We import the semaphores
 *   from the the master controller. Every interrupt will have an
 *   associated global ec waiting on the semaphore.
 * - An MMIO range that will be mapped in the VM.
 */
class Passthru::Device {
public:
    Device(const char *guest_dev, const char *host_dev, Model::Gic_d *gic)
        : _guest_dev(guest_dev), _host_dev(host_dev), _irqs(nullptr), _num_irqs(0),
          _io_ranges(nullptr), _num_ranges(0), _gic(gic) {}
    Device()
        : _guest_dev(nullptr), _host_dev(nullptr), _irqs(nullptr), _num_irqs(0),
          _io_ranges(nullptr), _num_ranges(0), _gic(nullptr) {}
    ~Device();
    Errno init(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree);

private:
    bool assert_irq(uint32 virt_irq_id) const { return _gic->assert_spi(virt_irq_id); }

    struct Irq_Entry {
        Device *device; // This device - used in the static function
        Irq_translation irq;
        Sel sm;
    };

    Errno init_ioranges(const Fdt::Tree &tree, const char *path);
    Errno init_irqs(const Fdt::Tree &tree, const char *path);
    bool has_gic_parent(const Fdt::Tree &tree, const char *path);
    Errno map_ioranges(const Zeta::Zeta_ctx *ctx, const char *path);
    Errno attach_irqs(const Zeta::Zeta_ctx *ctx);
    Errno assign_dev(const Zeta::Zeta_ctx *);

    Errno setup_interrupt_listener(const Zeta::Zeta_ctx *ctx, Irq_Entry &ent);
    static void wait_for_interrupt(const Zeta::Zeta_ctx *ctx, Irq_Entry *arg);

    const char *_guest_dev;
    const char *_host_dev;

    Irq_Entry *_irqs;
    size_t _num_irqs;
    Range<uint64> *_io_ranges;
    size_t _num_ranges{0};
    Model::Gic_d *const _gic;

    static cxx::Bitset<Model::MAX_IRQ> _irq_configured;
};
