/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <alloc/sels.hpp>
#include <alloc/vmap.hpp>
#include <bedrock/fdt.hpp>
#include <bitset.hpp>
#include <compiler.hpp>
#include <errno.hpp>
#include <fdt/as.hpp>
#include <fdt/property.hpp>
#include <log/log.hpp>
#include <model/passthru.hpp>
#include <new.hpp>
#include <zeta/zeta.hpp>

cxx::Bitset<Model::MAX_IRQ> Passthru::Device::_irq_configured;

__NORETURN__ void
Passthru::Device::wait_for_interrupt(const Zeta::Zeta_ctx *, Irq_Entry *irq_entry) {
    INFO("GEC interrupt waiter for device %p is ready: Physical %lu -> Virtual %lu",
         irq_entry->device, irq_entry->irq.phys_intr, irq_entry->irq.virt_intr);

    while (1) {
        Errno const err = Zeta::sm_down(irq_entry->sm);
        if (err != Errno::ENONE) {
            WARN("sm_down failed with errno %d", err);
            continue;
        }

        if (!irq_entry->device->assert_irq(irq_entry->irq.virt_intr)) {
            WARN("SPI assertion failed on GIC");
            continue;
        }
    }
}

Passthru::Device::~Device() {
    if (_irqs != nullptr) {
        delete[] _irqs;
    }
    if (_io_ranges != nullptr) {
        delete[] _io_ranges;
    }
}

Errno
Passthru::Device::setup_interrupt_listener(Cpu cpu, Irq_Entry &ent) {
    ent.device = this;

    return _interrupt_listener.start(
        cpu, Nova::Qpd(), reinterpret_cast<Zeta::global_ec_entry>(wait_for_interrupt), &ent);
}

Errno
Passthru::Device::init_ioranges(const Fdt::Tree &tree, const char *path) {
    Fdt::Prop::Reg_list_iterator regs;
    uint32 i;

    if (!fdt_device_regs_from_path(tree, regs, path)) {
        INFO("%s: No memory range mapping required", path);
        return ENONE;
    }

    _num_ranges = regs.num_elements_left();
    _io_ranges = new (nothrow) Range<uint64>[_num_ranges];
    if (_io_ranges == nullptr)
        return Errno::ENOMEM;

    for (i = 0; i < static_cast<uint32>(_num_ranges); i++, ++regs)
        _io_ranges[i] = Range<uint64>(regs.get_address(), regs.get_size());

    return Errno::ENONE;
}

bool
Passthru::Device::has_gic_parent(const Fdt::Tree &tree, const char *path) {
    /* Check interrupt parent to be gic */
    Fdt::Node dev_node = tree.lookup_from_path(path);
    if (!dev_node.is_valid()) {
        WARN("%s: node doesn't exist.", path);
        return false;
    }

    /* Get interrupt parent */
    Fdt::Node intr_parent = tree.lookup_interrupt_parent(dev_node);
    if (!intr_parent.is_valid()) {
        WARN("%s: cannot find interrupt-parent.", path);
        return false;
    }

    /* Check the compatibility strings to be gic */
    Fdt::Property comp_prpt = tree.lookup_property(intr_parent, "compatible");
    Fdt::Prop::Compatible compatible(comp_prpt);
    if (!compatible.is_valid())
        return false;

    Fdt::Prop::Property_str_list_iterator it(compatible.get_first_addr(),
                                             compatible.get_end_addr());
    const char *gic_compatible
        = (_gic->version() == Model::GIC_V2) ? Fdt::GIC_V2_COMPAT_NAME : Fdt::GIC_V3_COMPAT_NAME;
    for (; it.is_valid(); ++it) {
        if (!strcmp(it.get<const char *>(), gic_compatible)) {
            return true;
        }
    }
    return false;
}

Errno
Passthru::Device::init_irqs(const Fdt::Tree &tree, const char *path) {

    if (!has_gic_parent(tree, path)) {
        INFO("%s: Skipping non-gic interrupts.", path);
        return ENONE;
    }

    Fdt::Prop::Interrupts_list_iterator intr_list;
    if (!fdt_device_irqs_from_path(tree, intr_list, path)) {
        INFO("%s: No IRQ associated with the device", path);
        return ENONE;
    }

    ASSERT(intr_list.has_type());

    _num_irqs = intr_list.num_elements_left();
    _irqs = new (nothrow) Irq_Entry[_num_irqs];
    if (_irqs == nullptr)
        return Errno::ENOMEM;

    for (uint32 i = 0; i < static_cast<uint32>(_num_irqs); i++, ++intr_list) {
        if (intr_list.get_type() != Fdt::Prop::Interrupts_list_iterator::SPI) {
            WARN("%s: only SPIs interrupts are supported %s [%d] (%d, 0x%x)", path, _host_dev, i,
                 intr_list.get_type(), intr_list.get_irq());
            return Errno::EINVAL;
        }

        _irqs[i].irq.virt_intr = intr_list.get_irq() + Model::MAX_SGI + Model::MAX_PPI;
        /* XXX: device manager interface hides physical irq values. Assume it is equal to
         * guest */
        _irqs[i].irq.phys_intr = _irqs[i].irq.virt_intr;
        ASSERT(_irqs[i].irq.virt_intr < Model::MAX_IRQ);
        ASSERT(_irqs[i].irq.phys_intr < Model::MAX_IRQ);
    }

    return ENONE;
}

Errno
Passthru::Device::map_ioranges(const Zeta::Zeta_ctx *ctx, const char *path) {
    uint32 i;
    Errno err;

    for (i = 0; i < static_cast<uint32>(_num_ranges); i++) {
        Sel guest_va(_io_ranges[i].begin());

        DEBUG("Acquire resource %s : addr 0x%llx - size 0x%llx", _host_dev, _io_ranges[i].start(),
              _io_ranges[i].size());

        /* XXX: Acquiring by index. Assuming resources have the same order as in platform DT */
        err = Zeta::IO::acquire_resource(ctx, _host_dev, Zeta::API::RES_REG, i, guest_va,
                                         ctx->cpu(), true);
        if (err != Errno::ENONE) {
            WARN("%s: Cannot acquire mmio register %s [%d] (%llx): %d", path, _host_dev, i,
                 guest_va, err);
            return err;
        }
        INFO("%s: mapping 0x%llx with size 0x%llx", path, _io_ranges[i].begin(),
             _io_ranges[i].size());
    }
    return Errno::ENONE;
}

Errno
Passthru::Device::attach_irqs(const Zeta::Zeta_ctx *ctx) {
    uint32 i;
    Errno err;

    for (i = 0; i < static_cast<uint32>(_num_irqs); i++) {
        if (Device::_irq_configured.is_set(_irqs[i].irq.phys_intr)) {
            INFO("%s: Physical IRQ %d already configured - skipping", _guest_dev,
                 _irqs[i].irq.phys_intr);
            continue;
        }

        Sel int_sm = Sels::alloc();
        _irqs[i].sm = int_sm;
        err = Zeta::IO::acquire_resource(ctx, _host_dev, Zeta::API::RES_IRQ, i, _irqs[i].sm,
                                         ctx->cpu(), true);
        if (err != Errno::ENONE) {
            WARN("%s: Cannot acquire irq %s[%d] (0x%x): %d", __func__, _guest_dev, i,
                 _irqs[i].irq.phys_intr, err);
            return err;
        }
        Device::_irq_configured.atomic_set(_irqs[i].irq.phys_intr);

        /*
         * At the moment, all passthrough devices are configured with edge-triggered IRQs regardless
         * of the config or reality at the hardware level. This is because, to emulate a
         * level-triggered interrupt, we would need to know the status of the line at the HW level.
         * NOVA doesn't provide that info for now.
         */
        _gic->config_spi(_irqs[i].irq.virt_intr, true /* hw */, uint16(_irqs[i].irq.phys_intr),
                         true);

        err = setup_interrupt_listener(ctx->cpu(), _irqs[i]);
        if (err != Errno::ENONE) {
            WARN("Unabled to configure IRQ entry %llu", i);
            return err;
        }
        INFO("%s: Physical IRQ %d configured", _guest_dev, _irqs[i].irq.phys_intr);
    }
    return Errno::ENONE;
}

Errno
Passthru::Device::assign_dev(const Zeta::Zeta_ctx *ctx) {
    return Zeta::IO::assign_dev(ctx, _host_dev, true);
}

Errno
Passthru::Device::init(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree) {
    Errno err;

    err = init_ioranges(tree, _guest_dev);
    if (err != Errno::ENONE)
        return err;
    err = init_irqs(tree, _guest_dev);
    if (err != Errno::ENONE)
        return err;
    err = map_ioranges(ctx, _guest_dev);
    if (err != Errno::ENONE)
        return err;
    err = attach_irqs(ctx);
    if (err != Errno::ENONE)
        return err;
    err = assign_dev(ctx);
    return err;
}
