/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <fdt/fdt.hpp>
#include <fdt/property.hpp>

namespace Fdt {

    static constexpr uint8 GIC_REG_DISTRIBUTOR_INTERFACE = 0;
    static constexpr uint8 GIC_REG_CPU_INTERFACE = 1;
    static constexpr uint8 GIC_REG_VCPU_INTERFACE = 3;

    static constexpr const char *TIMER_COMPAT_NAME = "arm,armv8-timer";
    static constexpr uint8 PTIMER_IRQ_IDX = 1;
    static constexpr uint8 VTIMER_IRQ_IDX = 2;

    static constexpr const char *GIC_V2_COMPAT_NAME = "arm,cortex-a15-gic";
    static constexpr const char *GIC_V3_COMPAT_NAME = "arm,gic-v3";

    bool fdt_find_memory(const Fdt::Tree &tree, Fdt::Prop::Reg_list_iterator &list);
    bool fdt_device_regs(const Fdt::Tree &tree, Fdt::Prop::Reg_list_iterator &list,
                         const char *compat_name);
    bool fdt_device_irqs(const Fdt::Tree &tree, Fdt::Prop::Interrupts_list_iterator &list,
                         const char *compat_name);

    bool fdt_device_regs_from_path(const Fdt::Tree &tree, Fdt::Prop::Reg_list_iterator &list,
                                   const char *path);
    bool fdt_device_irqs_from_path(const Fdt::Tree &tree, Fdt::Prop::Interrupts_list_iterator &list,
                                   const char *path);

    bool fdt_read_regs(const Fdt::Tree &tree, Fdt::Node &node, Fdt::Prop::Reg_list_iterator &list);
    bool fdt_read_irqs(const Fdt::Tree &tree, Fdt::Node &n,
                       Fdt::Prop::Interrupts_list_iterator &list);

    bool fdt_read_pcpu_config(const Fdt::Tree &tree, Fdt::Prop::Property_list_iterator &pcpu_list);
    bool fdt_is_64bit_guest(const Fdt::Tree &tree);

    uint16 fdt_get_numcpus(const Fdt::Tree &tree);
}
