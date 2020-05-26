/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

namespace Vmconfig {

    /**
     * This file provides string constants to be used within VM config device tree.
     * The general structure of config tree consists of following nodes:
     *
     * 1. General VM configuration
     * 2. Interrupt controller configuration (GICv2 only for now)
     * 3. PCPU <-> VCPU mapping
     * 4. Passthrough devices
     * 5. Virtio devices
     *
     * The passthrough block contains a node each for every passthrough device.
     *
     * The configuration options used by passthrough nodes are:
     *
     *  1. guest-path: Corresponds to the path of device node in guest device tree.
     *  2. host-path:  Corresponds to path of the device node in host device tree.
     *
     *  An example passthrough node containing a single passthrough ethernet device:
     *
     *  passthrough {
     *    ethernet {
     *     guest-path = "/some/path/ethernet";
     *     host-path  = "/some/path/ethernet";
     *    };
     *  };
     *
     * Virtio devices are configured within a virtio_devices block with each device node providing
     * configuration for one virtio device in guest tree.
     *
     * The configuration values currently used for virtio device node are:
     *
     *   1. guest-path: Provides the path of the corresponding virtio device in guest FDT.
     *   2. type:       Describes the virtio device type.
     *                  Currently "net" and "serial" type devices are supported.
     *   3. id:         Unique port identifier for "net" type nodes. Must be unique for each port in
     *the system.
     *
     *   An example virtio devices node containing serial and network virtio devices:
     *
     *   virtio_devices {
     *     serial1 {
     *       guest-path = "/some/path/virtio1";
     *       type = "serial";
     *     };
     *
     *     net1 {
     *       guest-path = "/some/path/virtio2";
     *       type = "net";
     *       id = <0x0>;
     *       mac = [AA BB CC DD EE FF];
     *       mtu = <1500>
     *     };
     *  };
     *
     * Additionally, the root node of the config tree will contain general information about
     * the VM. For example, it will tell if the VCPUs should start in AArch32 mode.
     *
     * For example:
     *
     * / {
     *     aarch32;
     * };
     *
     * will tell the VMM to start the VCPU in 32-bit mode.
     *
     * - Interrupt-controller: For GICv2, it is necessary to map a memory region of the GIC from
     * the host to the guest. In that case, we rely on a node in the config such as:
     *
     *  interrupt-controller {
     *    host-path = "/intc@8000000";
     *  };
     *
     * host-path should point to the GIC in the host FDT config.
     *
     * Address space configuration:
     *
     * Guest FDT address property: fdt-addr = <0x0 0x420000>
     * Guest kernel load address:  kernel-addr = <0x0 0x430000>
     * Guest ROM load address: bootrom-addr = <0x0 0x0>
     * Guest program counter at boot: pc-boot-addr = <0x0 0x0>
     *
     * If no address space config is provided, the vmm will use reasonable default
     * values. Here is an example of a valid configuration:
     *
     * guest {
     *     fdt-addr = <0x0 0x420000>;
     *     kernel-addr = <0x0 0x430000>;
     *     bootrom-addr = <0x0 0x0>;
     *     pc-boot-addr = <0x0 0x0>;
     * };
     *
     **/

    // It identifies the node containing passthrough devices.
    static constexpr char const* VCPUS_NODE = "/vcpus";

    // It identifies the node containing passthrough devices.
    static constexpr char const* VCPUS_MAPPING_PROP = "mapping";

    // It identifies the node containing passthrough devices.
    static constexpr char const* PASSTHROUGH = "/passthrough";

    // It idenitifes the node containing virtio devices.
    static constexpr char const* VIRTIO_DEVICES = "/virtio_devices";

    // This corresponds to the node path in guest device tree
    static constexpr char const* GUEST_PATH = "guest-path";

    // This corresponds to the node path in host device tree
    static constexpr char const* HOST_PATH = "host-path";

    // This entry is used to identify virtio device type.
    static constexpr char const* VIRTIO_TYPE = "type";

    // Virtio net device type.
    static constexpr char const* VIRTIO_NET = "net";

    // Virtio serial device type.
    static constexpr char const* VIRTIO_SERIAL = "serial";

    // Unique port ID for net type node.
    static constexpr char const* PORT_ID = "id";

    // MAC address for virtio net device.
    static constexpr char const* MAC = "mac";

    // MTU for virtio net device.
    static constexpr char const* MTU = "mtu";

    // Should we start the VCPUs in AArch32 mode?
    static constexpr char const* AARCH32 = "aarch32";

    // Interrupt controller configuration
    static constexpr char const* INTR_CTRL = "/interrupt-controller";

    // It idenitifes the node containing SCMI firmware.
    static constexpr char const* SCMI_FIRMWARE = "/firmware/scmi";

    // The compatibility string for ARM SCMI over SMC transport.
    static constexpr char const* SCMI_ARM_SMCC = "arm,scmi-smc";

    // SMC ID used by the guest for SCMI notifications.
    static constexpr char const* SCMI_ARM_SMCID = "arm,smc-id";

    // This entry is used to identify the SCMI shared memory config.
    static constexpr char const* SCMI_SHMEM = "shmem";

    static constexpr char const* GUEST_AS_NODE_PATH = "/guest";

    // Guest address space configuration
    static constexpr char const* GUEST_FDT_ADDR_PROP = "fdt-addr";
    static constexpr char const* GUEST_KERNEL_ADDR_PROP = "kernel-addr";
    static constexpr char const* GUEST_BOOTROM_ADDR_PROP = "bootrom-addr";
    static constexpr char const* GUEST_PC_BOOT_ADDR_PROP = "pc-boot-addr";

    extern const char* name;
};
