/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <bedrock/fdt.hpp>
#include <fdt/as.hpp>
#include <fdt/fdt.hpp>
#include <fdt/property.hpp>
#include <fdt/uart.hpp>
#include <log/log.hpp>
#include <vm_config.hpp>
#include <zeta/zeta.hpp>

bool
Fdt::fdt_read_regs(const Fdt::Tree &tree, Fdt::Node &node, Fdt::Prop::Reg_list_iterator &it) {
    Fdt::Property prop = tree.lookup_property(node, Fdt::Prop::Reg_list::name);
    if (!prop.is_valid())
        return false;

    Fdt::Node parent = tree.lookup_parent(node);
    if (!parent.is_valid())
        return false;

    Fdt::As::Address_space as(tree, parent);

    Fdt::Prop::Reg_list list(prop);
    if (!list.is_valid())
        return false;

    it = Fdt::Prop::Reg_list_iterator(list.get_first_addr(), list.get_end_addr(),
                                      as.get_addr_cells(), as.get_size_cells());

    return true;
}

bool
Fdt::fdt_read_irqs(const Fdt::Tree &tree, Fdt::Node &n, Fdt::Prop::Interrupts_list_iterator &it) {
    Fdt::Property interrupts = tree.lookup_property(n, Fdt::Prop::Interrupts_list::name);
    if (!interrupts.is_valid())
        return false;

    Fdt::Num_cell cells = tree.lookup_interrupt_cells(n);
    if (cells.get_num_cells() == 0)
        return false;

    Fdt::Prop::Interrupts_list irq_list(interrupts);
    if (!irq_list.is_valid())
        return false;

    it = Fdt::Prop::Interrupts_list_iterator(irq_list.get_first_addr(), irq_list.get_end_addr(),
                                             cells);

    return true;
}

bool
Fdt::fdt_find_memory(const Fdt::Tree &tree, Fdt::Prop::Reg_list_iterator &list) {
    Node memory_node;
    size_t devices_found;

    devices_found = tree.lookup_with_device_type("memory", &memory_node, 1);
    ASSERT(devices_found == 1);
    ASSERT(memory_node.is_valid());

    return fdt_read_regs(tree, memory_node, list);
}

bool
Fdt::fdt_device_regs(const Fdt::Tree &tree, Fdt::Prop::Reg_list_iterator &list,
                     const char *compat_name) {
    Fdt::Node node(tree.lookup_compatible_device(compat_name));
    if (!node.is_valid())
        return false;
    return fdt_read_regs(tree, node, list);
}

bool
Fdt::fdt_device_irqs(const Fdt::Tree &tree, Fdt::Prop::Interrupts_list_iterator &list,
                     const char *compat_name) {
    Fdt::Node node(tree.lookup_compatible_device(compat_name));
    if (!node.is_valid())
        return false;
    return fdt_read_irqs(tree, node, list);
}

bool
Fdt::fdt_device_regs_from_path(const Fdt::Tree &tree, Fdt::Prop::Reg_list_iterator &list,
                               const char *path) {
    Fdt::Node node(tree.lookup_from_path(path));
    if (!node.is_valid())
        return false;
    return fdt_read_regs(tree, node, list);
}

bool
Fdt::fdt_device_irqs_from_path(const Fdt::Tree &tree, Fdt::Prop::Interrupts_list_iterator &list,
                               const char *path) {
    Fdt::Node node(tree.lookup_from_path(path));
    if (!node.is_valid())
        return false;
    return fdt_read_irqs(tree, node, list);
}

bool
Fdt::fdt_read_pcpu_config(const Fdt::Tree &tree, Fdt::Prop::Property_list_iterator &pcpu_list) {
    Fdt::Node node(tree.lookup_from_path(Vmconfig::VCPUS_NODE));
    if (!node.is_valid())
        return false;

    Fdt::Property prop(tree.lookup_property(node, Vmconfig::VCPUS_MAPPING_PROP));
    Fdt::Prop::Property_list list(prop);
    if (!list.is_valid())
        return false;

    pcpu_list = Fdt::Prop::Property_list_iterator(list.get_first_addr(), list.get_end_addr(),
                                                  sizeof(uint32));
    return true;
}

bool
Fdt::fdt_is_64bit_guest(const Fdt::Tree &tree) {
    Fdt::Node root(tree.get_root());
    if (!root.is_valid())
        return true;

    Fdt::Property prop(tree.lookup_property(root, Vmconfig::AARCH32));
    return !prop.is_valid();
}

uint16
Fdt::fdt_get_numcpus(const Fdt::Tree &tree) {
    Fdt::Node node;
    return static_cast<uint16>(tree.lookup_with_device_type("cpu", &node, 1));
}
