/**
 * Copyright (c) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <bedrock/fdt.hpp>
#include <bedrock/umx_interface.hpp>
#include <bedrock/vswitch_interface.hpp>
#include <fdt/as.hpp>
#include <fdt/property.hpp>
#include <log/log.hpp>
#include <model/board.hpp>
#include <model/cpu.hpp>
#include <model/guest_as.hpp>
#include <model/passthru.hpp>
#include <model/platform_device.hpp>
#include <model/platform_firmware.hpp>
#include <new.hpp>
#include <pl011/pl011.hpp>
#include <platform/semaphore.hpp>
#include <pm_client.hpp>
#include <zeta/zeta.hpp>

static constexpr const uint16 VIRTIO_NET_QUEUE_SIZE = 1024;

// Private implementation of the board
class Model::Board_impl {
private:
    struct Devices {
        Passthru::Device *passthru_devices{nullptr};
        Pm_client plat_mgr;
    };

    Devices *_all_devices;

    Gic_version _gic_version{GIC_UNKNOWN};

public:
    Vbus::Bus device_bus;
    Model::Gic_d *gic_d;

    Model::Firmware *firmware{nullptr};

    Model::Guest_as *guest_as;

public:
    Errno init(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree, Model::Guest_as &ram_as,
               Model::Guest_as &rom_as, const mword guest_config_addr, const Uuid &umx_uuid,
               const Uuid &vswitch_uuid, const Uuid &plat_mgr_uuid);
    Errno setup_gic(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &config,
                    const Fdt::Tree &guest_tree);
    Errno setup_virtio_console(const Zeta::Zeta_ctx *ctx, uint64 const guest_base,
                               uint64 const vmm_base, uint64 const mem_size, const Uuid &umx_uuid,
                               Fdt::Prop::Reg_list_iterator &regs,
                               Fdt::Prop::Interrupts_list_iterator &intrs);
    Errno setup_pl011_console(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                              const Uuid &umx_uuid);
    Errno setup_debug_console(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                              const Uuid &umx_uuid);
    Errno setup_virtio_ethernet(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &config_tree,
                                Fdt::Node &n, uint64 const guest_base, uint64 const vmm_base,
                                uint64 const mem_size, const Uuid &vswitch_uuid,
                                Fdt::Prop::Reg_list_iterator &regs,
                                Fdt::Prop::Interrupts_list_iterator &intrs);
    Errno setup_passthru_devices(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                                 const Fdt::Tree &config_tree);
    Errno setup_virtio_device(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                              const Fdt::Tree &config_tree, Fdt::Node &n, uint64 const guest_base,
                              uint64 const vmm_base, uint64 const mem_size, const Uuid &umx_uuid,
                              const Uuid &vswitch_uuid);
    Errno setup_virtio_devices(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                               uint64 const guest_base, uint64 const vmm_base,
                               uint64 const mem_size, const Uuid &umx_uuid,
                               const Uuid &vswitch_uuid, const Fdt::Tree &config_tree);
    Errno setup_platform_devices(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                                 const Fdt::Tree &config_tree, const Uuid &plat_mgr_uuid);
};

Errno
Model::Board::init(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree, Model::Guest_as &ram_as,
                   Model::Guest_as &rom_as, const mword guest_config_addr, const Uuid &umx_uuid,
                   const Uuid &vswitch_uuid, const Uuid &plat_mgr_uuid) {
    impl = new (nothrow) Board_impl;
    if (impl == nullptr)
        return ENOMEM;

    return impl->init(ctx, tree, ram_as, rom_as, guest_config_addr, umx_uuid, vswitch_uuid,
                      plat_mgr_uuid);
}

Errno
Model::Board_impl::init(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree, Model::Guest_as &ram_as,
                        Model::Guest_as &rom_as, const mword guest_config_addr,
                        const Uuid &umx_uuid, const Uuid &vswitch_uuid, const Uuid &plat_mgr_uuid) {
    Errno err;

    if (guest_config_addr == 0) {
        ABORT_WITH("No VM config provided. We cannot continue");
    }

    const Fdt::Tree *config_tree = new (reinterpret_cast<char *>(guest_config_addr)) Fdt::Tree();
    if (config_tree->validate() != Fdt::Tree::Format_err::OK) {
        ABORT_WITH("Invalid VM config provided. We cannot continue");
    }

    guest_as = &ram_as;
    device_bus.register_device(&ram_as, ram_as.get_guest_view().get_value(), ram_as.get_size());

    if (rom_as.get_size() != 0)
        device_bus.register_device(&rom_as, rom_as.get_guest_view().get_value(), rom_as.get_size());

    _all_devices = new (nothrow) Devices;
    ASSERT(_all_devices != nullptr);

    err = setup_gic(ctx, *config_tree, tree);
    if (err != ENONE)
        return err;

    err = setup_debug_console(ctx, tree, umx_uuid);
    if (err != ENONE)
        return err;

    err = setup_virtio_devices(ctx, tree, ram_as.get_guest_view().get_value(),
                               reinterpret_cast<mword>(ram_as.get_vmm_view()), ram_as.get_size(),
                               umx_uuid, vswitch_uuid, *config_tree);
    if (err != ENONE)
        return err;

    err = setup_passthru_devices(ctx, tree, *config_tree);
    if (err != ENONE)
        return err;

    err = setup_platform_devices(ctx, tree, *config_tree, plat_mgr_uuid);
    if (err != ENONE)
        return err;
    return ENONE;
}

Errno
Model::Board_impl::setup_passthru_devices(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                                          const Fdt::Tree &config_tree) {
    Errno err;
    unsigned num_devices = 0, dev_idx = 0;
    Fdt::Node passthr_root(config_tree.lookup_from_path(Vmconfig::PASSTHROUGH));
    if (!passthr_root.is_valid()) {
        INFO("No passthrough device configured - skipping");
        return ENONE;
    }

    Fdt::Node first_node(passthr_root.get_first_child());
    Fdt::Node n = first_node;

    for (; n.is_valid(); n = n.get_sibling()) {
        num_devices++;
    }

    INFO("Found %u device(s) to passthrough", num_devices);
    _all_devices->passthru_devices = new (nothrow) Passthru::Device[num_devices];
    if (_all_devices->passthru_devices == nullptr)
        return Errno::ENOMEM;

    for (n = first_node; n.is_valid(); n = n.get_sibling()) {
        Fdt::Property guest_prop(config_tree.lookup_property(n, Vmconfig::GUEST_PATH));
        Fdt::Property host_prop(config_tree.lookup_property(n, Vmconfig::HOST_PATH));

        if (!guest_prop.is_valid() || !host_prop.is_valid()) {
            FATAL("Unable to find read the guest or host path");
            return EINVAL;
        }
        const char *guest_dev = guest_prop.get_data<const char *>();
        const char *host_dev = host_prop.get_data<const char *>();

        INFO("%s Acquiring passthru device %s -> %s", n.get_name(), guest_dev, host_dev);
        Passthru::Device *dev = new (&(_all_devices->passthru_devices[dev_idx++]))
            Passthru::Device(guest_dev, host_dev, gic_d);
        err = dev->init(ctx, tree);
        if (err != Errno::ENONE)
            break;
    }

    return ENONE;
}

Errno
Model::Board_impl::setup_virtio_console(const Zeta::Zeta_ctx *ctx, uint64 const guest_base,
                                        uint64 const vmm_base, uint64 const mem_size,
                                        const Uuid &umx_uuid, Fdt::Prop::Reg_list_iterator &regs,
                                        Fdt::Prop::Interrupts_list_iterator &intrs) {
    Errno err;
    Model::Virtio_console *console;

    Semaphore *sem = new (nothrow) Semaphore();
    if (!sem) {
        return ENOMEM;
    }

    if (!sem->init(ctx)) {
        return ENOMEM;
    }

    console = new Virtio_console(*gic_d, guest_base, vmm_base, mem_size,
                                 static_cast<uint16>(MAX_SGI + MAX_PPI + intrs.get_irq()), 8, sem);
    ASSERT(console != nullptr);

    Umx::Connection_helper *connection_helper = new Umx::Connection_helper();
    if (connection_helper == nullptr) {
        delete console;
        delete sem;
        return ENOMEM;
    }

    err = connection_helper->init(ctx, Umx::GUEST_DEFAULT_TX_SIZE, Umx::GUEST_DEFAULT_RX_SIZE);
    if (err != ENONE) {
        delete console;
        delete connection_helper;
        delete sem;
        return err;
    }

    Umx::Virtio_backend *umx_backend = new Umx::Virtio_backend(console, connection_helper, sem);
    ASSERT(umx_backend != nullptr);

    console->register_callback(*umx_backend);
    err = umx_backend->setup_umx_virtio_bridge(ctx, umx_uuid, Vmconfig::name);
    if (err != ENONE) {
        WARN("Unable to connect to UMX. Virtio console will be disabled");
        delete console;
        delete connection_helper;
        delete umx_backend;
        delete sem;
        return err;
    }

    device_bus.register_device(console, regs.get_address(), regs.get_size());

    return ENONE;
}

Errno
Model::Board_impl::setup_pl011_console(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                                       const Uuid &umx_uuid) {
    const char *compat_name = "arm,pl011";
    Fdt::Prop::Reg_list_iterator regs;
    Fdt::Prop::Interrupts_list_iterator intrs;
    Model::Pl011 *pl011;
    Errno err;

    if (!Fdt::fdt_device_regs(tree, regs, compat_name)) {
        return ENODEV;
    }

    ASSERT(regs.num_elements_left() == 1);

    if (!Fdt::fdt_device_irqs(tree, intrs, compat_name)) {
        WARN("Pl011 entry in guest FDT with no irq entry");
        return ENODEV;
    }

    if (regs.num_elements_left() == 0) {
        WARN("Incorrect IRQ configuration for the PL011");
        return ENODEV;
    }

    uint16 irq_id = static_cast<uint16>(MAX_SGI + MAX_PPI + intrs.get_irq());
    bool edge = true;

    if (intrs.has_flags())
        edge = intrs.get_flags() & Fdt::Prop::Interrupts_list_iterator::EDGE;

    gic_d->config_spi(irq_id, false, 0, edge);
    pl011 = new Model::Pl011(*gic_d, irq_id);
    if (pl011 == nullptr)
        return ENOMEM;

    Umx::Connection_helper *connection_helper = new Umx::Connection_helper();
    if (connection_helper == nullptr) {
        delete pl011;
        return ENOMEM;
    }

    err = connection_helper->init(ctx, Umx::GUEST_DEFAULT_TX_SIZE, Umx::GUEST_DEFAULT_RX_SIZE);
    if (err != ENONE) {
        delete pl011;
        delete connection_helper;
        return err;
    }

    Umx::Pl011_backend *backend = new Umx::Pl011_backend(pl011, connection_helper);
    ASSERT(backend != nullptr);

    pl011->register_callback(backend);
    err = backend->setup_umx_pl011_bridge(ctx, umx_uuid, Vmconfig::name);
    if (err != ENONE) {
        WARN("Unable to connect to UMX. PL011 console will be disabled");
        delete pl011;
        delete connection_helper;
        delete backend;
        return err;
    }

    device_bus.register_device(pl011, regs.get_address(), regs.get_size());

    return ENONE;
}

Errno
Model::Board_impl::setup_debug_console(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                                       const Uuid &umx_uuid) {
    Errno err;

    err = setup_pl011_console(ctx, tree, umx_uuid);
    if (err != ENONE) {
        INFO("No PL011 configured")
    } else {
        INFO("PL011 configured.");
    }

    return ENONE;
}

Errno
Model::Board_impl::setup_virtio_ethernet(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &config_tree,
                                         Fdt::Node &n, uint64 const guest_base,
                                         uint64 const vmm_base, uint64 const mem_size,
                                         const Uuid &vswitch_uuid,
                                         Fdt::Prop::Reg_list_iterator &regs,
                                         Fdt::Prop::Interrupts_list_iterator &intrs) {

    uint32 device_feature = 0;
    uint64 mac = 0;
    uint16 mtu = 0;

    Fdt::Property port_id_prop(config_tree.lookup_property(n, Vmconfig::PORT_ID));
    if (!port_id_prop.is_valid()) {
        WARN("Invalid port ID on virtio net node. skipping");
        return ENODEV;
    }
    uint16 port_id = static_cast<uint16>(port_id_prop.get_data<uint32>());

    Fdt::Property mac_prop(config_tree.lookup_property(n, Vmconfig::MAC));
    if (mac_prop.is_valid()) {
        device_feature |= VIRTIO_NET_MAC;
        const char *mac_array = mac_prop.get_data<const char *>();
        mac = static_cast<uint64>(mac_array[6]) << 48 | static_cast<uint64>(mac_array[5]) << 40
              | static_cast<uint64>(mac_array[4]) << 32 | static_cast<uint64>(mac_array[3]) << 24
              | static_cast<uint64>(mac_array[2]) << 16 | static_cast<uint64>(mac_array[1]) << 8
              | static_cast<uint64>(mac_array[0]);
    }

    Fdt::Property mtu_prop(config_tree.lookup_property(n, Vmconfig::MTU));
    if (mtu_prop.is_valid()) {
        device_feature |= VIRTIO_NET_MTU;
        mtu = static_cast<uint16>(mac_prop.get_data<uint32>());
    }

    Sel sm_sel = Sels::alloc();
    if (sm_sel == Sels::INVALID) {
        return ENOMEM;
    }

    Semaphore *sem = new (nothrow) Semaphore();
    if (!sem) {
        return ENOMEM;
    }

    sem->init(ctx);

    Model::Virtio_net *network;

    network = new Virtio_net(*gic_d, guest_base, vmm_base, mem_size,
                             static_cast<uint16>(MAX_SGI + MAX_PPI + intrs.get_irq()),
                             VIRTIO_NET_QUEUE_SIZE, device_feature, mac, mtu, sem);
    ASSERT(network != nullptr);

    device_bus.register_device(network, regs.get_address(), regs.get_size());

    if (__UNLIKELY__(vswitch_uuid == Uuid::NULL)) {
        WARN("Virtio ethernet configured but no vswitch to connect to.")
        return ENONE;
    }

    VSwitch::Virtio_backend *backend = new VSwitch::Virtio_backend(
        ctx, vswitch_uuid, vmm_base, guest_base, mem_size, network, port_id, sm_sel, sem);
    ASSERT(backend != nullptr);

    network->register_callback(*backend);

    if (backend->setup_listeners(ctx) != ENONE) {
        WARN("Unable to setup network listeners.");
        return ENODEV;
    }

    return ENONE;
}

Errno
Model::Board_impl::setup_virtio_device(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                                       const Fdt::Tree &config_tree, Fdt::Node &n,
                                       uint64 const guest_base, uint64 const vmm_base,
                                       uint64 const mem_size, const Uuid &umx_uuid,
                                       const Uuid &vswitch_uuid) {

    Fdt::Prop::Reg_list_iterator regs;
    Fdt::Prop::Interrupts_list_iterator intrs;

    Fdt::Property guest_prop(config_tree.lookup_property(n, Vmconfig::GUEST_PATH));
    Fdt::Property type_prop(config_tree.lookup_property(n, Vmconfig::VIRTIO_TYPE));
    if (!guest_prop.is_valid() || !type_prop.is_valid()) {
        WARN("Unable to find read the guest path or device type");
        return ENODEV;
    }

    const char *virtio_dev = guest_prop.get_data<const char *>();
    const char *type = type_prop.get_data<const char *>();

    Fdt::Node virtio_node = tree.lookup_from_path(virtio_dev);

    if (!virtio_node.is_valid()) {
        WARN("Invalid virtio entry in guest FDT.");
        return ENODEV;
    }

    if (!Fdt::fdt_read_regs(tree, virtio_node, regs)) {
        WARN("Virtio entry in guest FDT with no reg entry");
        return ENODEV;
    }

    ASSERT(regs.num_elements_left() == 1);

    if (!Fdt::fdt_read_irqs(tree, virtio_node, intrs)) {
        WARN("Virtio entry in guest FDT with no irq entry");
        return ENODEV;
    }

    ASSERT(intrs.num_elements_left() == 1);

    Errno err = ENONE;
    if (!strncmp(type, Vmconfig::VIRTIO_NET, strlen(Vmconfig::VIRTIO_NET))
        && (strlen(type) == strlen(Vmconfig::VIRTIO_NET))) {
        err = setup_virtio_ethernet(ctx, config_tree, n, guest_base, vmm_base, mem_size,
                                    vswitch_uuid, regs, intrs);
    } else if (!strncmp(type, Vmconfig::VIRTIO_SERIAL, strlen(Vmconfig::VIRTIO_SERIAL))
               && (strlen(type) == strlen(Vmconfig::VIRTIO_SERIAL))) {
        err = setup_virtio_console(ctx, guest_base, vmm_base, mem_size, umx_uuid, regs, intrs);
    } else {
        WARN("Device type is currently not supported");
    }

    return err;
}

Errno
Model::Board_impl::setup_virtio_devices(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree,
                                        uint64 const guest_base, uint64 const vmm_base,
                                        uint64 const mem_size, const Uuid &umx_uuid,
                                        const Uuid &vswitch_uuid, const Fdt::Tree &config_tree) {
    Fdt::Node virtio_root(config_tree.lookup_from_path(Vmconfig::VIRTIO_DEVICES));
    if (!virtio_root.is_valid()) {
        INFO("No virtio devices configured - skipping");
        return ENONE;
    }

    for (Fdt::Node n(virtio_root.get_first_child()); n.is_valid(); n = n.get_sibling()) {
        Errno err = setup_virtio_device(ctx, tree, config_tree, n, guest_base, vmm_base, mem_size,
                                        umx_uuid, vswitch_uuid);
        if (err != ENONE)
            WARN("Virtio device initialization failed.");
    }

    return ENONE;
}

static Errno
setup_gicv2_resource(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &config_tree,
                     Fdt::Prop::Reg_list_iterator regs) {
    Errno err = ENXIO;
    uint64 guest_gic_cpu_addr = 0;

    if (regs.num_elements_left() <= Fdt::GIC_REG_CPU_INTERFACE)
        return Errno::EINVAL;

    Fdt::Node intr_ctl(config_tree.lookup_from_path(Vmconfig::INTR_CTRL));
    if (!intr_ctl.is_valid()) {
        ABORT_WITH("No interrupt controller node in the config. Unable to configure the VM");
    }

    Fdt::Property intr_ctrl_name = config_tree.lookup_property(intr_ctl, Vmconfig::HOST_PATH);
    if (!intr_ctrl_name.is_valid()) {
        ABORT_WITH("No host-path property in the interrupt-controller node.");
    }

    regs += Fdt::GIC_REG_CPU_INTERFACE;
    guest_gic_cpu_addr = regs.get_address();

    Sel guest_va(guest_gic_cpu_addr);
    err = Zeta::IO::acquire_resource(ctx, intr_ctrl_name.get_data<const char *>(),
                                     Zeta::API::RES_REG, Fdt::GIC_REG_VCPU_INTERFACE, guest_va,
                                     ctx->cpu(), true);
    return err;
}

Errno
Model::Board_impl::setup_gic(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &config,
                             const Fdt::Tree &guest_tree) {
    Fdt::Prop::Reg_list_iterator regs;
    bool ok;

    ok = Fdt::fdt_device_regs(guest_tree, regs, Fdt::GIC_V2_COMPAT_NAME);
    if (ok) {
        Errno err;

        _gic_version = GIC_V2;
        err = setup_gicv2_resource(ctx, config, regs);
        ASSERT(err == ENONE);
    } else {
        ok = Fdt::fdt_device_regs(guest_tree, regs, Fdt::GIC_V3_COMPAT_NAME);

        ASSERT(regs.num_elements_left() > Fdt::GIC_REG_CPU_INTERFACE);

        _gic_version = GIC_V3;
    }
    ASSERT(ok);

    INFO("VM configured to use GIC version %u", _gic_version);

    ASSERT(regs.num_elements_left() > Fdt::GIC_REG_DISTRIBUTOR_INTERFACE);

    gic_d = new Model::Gic_d(_gic_version, Fdt::fdt_get_numcpus(guest_tree));
    if (gic_d == nullptr) {
        ABORT_WITH("Unable to allocate memory for the GICD");
    } else {
        ok = gic_d->init();
        if (!ok)
            ABORT_WITH("Unable to init the GICD");
    }

    regs += Fdt::GIC_REG_DISTRIBUTOR_INTERFACE;
    INFO("GICD configured @ 0x%llx", regs.get_address());
    device_bus.register_device(gic_d, regs.get_address(), regs.get_size());

    return ENONE;
}

Errno
Model::Board_impl::setup_platform_devices(const Zeta::Zeta_ctx *, const Fdt::Tree &tree,
                                          const Fdt::Tree &config_tree, const Uuid &plat_mgr_uuid) {
    Fdt::Node plat_root(config_tree.lookup_from_path("/platform"));
    if (!plat_root.is_valid()) {
        INFO("No platform devices - skipping");
        return ENONE;
    }

    /* initialize client of platform manager */
    Errno err = _all_devices->plat_mgr.init(plat_mgr_uuid);
    if (err != ENONE) {
        ABORT_WITH("Cannot initialized Platform Manager client");
    }

    Fdt::Node firmware_node;
    Fdt::Node first_node(plat_root.get_first_child());
    Fdt::Node n = first_node;

    /* Get platform devices */
    for (; n.is_valid(); n = n.get_sibling()) {
        Fdt::Property comp_prpt = config_tree.lookup_property(n, "compatible");
        Fdt::Prop::Compatible compat(comp_prpt);

        if (!compat.is_valid()) {
            ABORT_WITH("Device %s doesn't have compatible property", n.get_name());
        }
        INFO("Checking platform device %s", n.get_name());

        Fdt::Prop::Property_str_list_iterator it(compat.get_first_addr(), compat.get_end_addr());
        for (; it.is_valid(); ++it) {
            if (!strcmp(it.get<const char *>(), "platform,device")) {
                Fdt::Property guest_prop(config_tree.lookup_property(n, "guest-deviceid"));
                if (!guest_prop.is_valid()) {
                    ABORT_WITH("guest-deviceid property is missing for a platform device %s.",
                               n.get_name());
                }

                const char *guest_dev = guest_prop.get_data<const char *>();
                Fdt::Node guest_node(tree.lookup_from_path(guest_dev));
                if (!guest_node.is_valid()) {
                    ABORT_WITH("Cannot find guest platform device %s.", guest_dev);
                }

                /* get guest_device registers  */
                Fdt::Prop::Reg_list_iterator regs;
                if (!Fdt::fdt_read_regs(tree, guest_node, regs)) {
                    /* no registers. invalid node */
                    ABORT_WITH("Guest platform device %s doesn't have regs.", guest_dev);
                }

                /* get reg_id for platform device  */
                Fdt::Property regid_prpt = config_tree.lookup_property(n, "reg_id");
                if (!regid_prpt.is_valid()) {
                    ABORT_WITH("reg_id property is missing for a platform device %s.",
                               n.get_name());
                }
                uint8 reg_id = static_cast<uint8>(regid_prpt.get_data<uint32>());

                INFO("Adding platform device %s reg_id %d (%llx, %x).", guest_dev, reg_id,
                     regs.get_address(), regs.get_size());

                Model::Platform_device *plat_device = new (nothrow)
                    Model::Platform_device(guest_dev, &_all_devices->plat_mgr, reg_id);
                if (plat_device == nullptr)
                    ABORT_WITH("Unable to allocate platform device %s", n.get_name());
                device_bus.register_device(plat_device, regs.get_address(), regs.get_size());
                break;
            } else if (!strcmp(it.get<const char *>(), "platform,firmware")) {
                firmware_node = n;
            }
        }
    }

    /* Initialize firmware */
    if (firmware_node.is_valid()) {
        INFO("Adding platform firmware to the board");
        firmware = new Model::Firmware(&_all_devices->plat_mgr);
        if (firmware == nullptr) {
            ABORT_WITH("Cannot allocate memory for firmware");
        }
    }

    return ENONE;
}

bool
Model::Board::setup_gicr(Model::Cpu &cpu, const Fdt::Tree &tree) {
    Fdt::Prop::Reg_list_iterator regs;
    uint64 start, size;

    if (impl->gic_d->version() == GIC_V2)
        return true;
    if (impl->gic_d->version() == GIC_UNKNOWN)
        return false;
    if (!Fdt::fdt_device_regs(tree, regs, Fdt::GIC_V3_COMPAT_NAME))
        return false;

    regs += Fdt::GIC_REG_CPU_INTERFACE;
    start = regs.get_address();
    size = regs.get_size();

    static constexpr size_t GICR_SIZE = 0x20000;

    INFO("GICR configured @ 0x%llx for VCPU %u", start + cpu.id() * GICR_SIZE, cpu.id());
    if (GICR_SIZE * (cpu.id() + 1) <= size)
        return impl->device_bus.register_device(cpu.gic_r(), start + cpu.id() * GICR_SIZE,
                                                GICR_SIZE);
    return false;
}

Model::Gic_d *
Model::Board::get_gic() const {
    return impl->gic_d;
}

Model::Guest_as *
Model::Board::get_ram() const {
    return impl->guest_as;
}

Model::Firmware *
Model::Board::get_firmware() const {
    return impl->firmware;
}

Vbus::Bus *
Model::Board::get_bus() const {
    return &impl->device_bus;
}
