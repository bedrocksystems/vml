/**
 * Copyright (C) 2020 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/types.hpp>
#include <platform/unique_ptr.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class CpuIrqInterface;
    class IrqController;
    class LocalIrqController;

    // Likely we will need to change this for x86
    enum Irqs {
        SGI_BASE = 0,
        MAX_SGI = 16,
        PPI_BASE = MAX_SGI,
        MAX_PPI = 16,
        SPI_BASE = PPI_BASE + MAX_PPI,
        MAX_SPI = 992,
        MAX_IRQ = 1024 - 4,
    };

    struct IrqInfo {
        bool active{false};
        bool pending{false};
        bool enabled{false};
        bool in_injection{false};
        uint8 priority{0};
        uint32 target{UINT32_MAX};
        uint64 num_asserted{0};
        uint64 num_acked{0};
        uint64 num_handled{0};
    };

    enum IRQCtlrVersion {
        // ARM
        GIC_UNKNOWN = 0,
        GIC_V2 = 2,
        GIC_V3 = 3,
        // Intel
        APIC,
        X2APIC,
    };

    // Data structure to track the VCPUs that an asserted IRQ can be routed to.
    // This can be used by passthrough code to rebalance IRQ handlers on the host side.
    // Updated by [assert_msi()].
    struct IrqAssertionRecord {
        // Array indexed by VCPU IDs. routed[i] is true if the IRQ is routed to VCPU i.
        unique_ptr<bool[]> routed;
        // Set by [IrqController] if [routed] changes.
        bool dirty{false};

        void update_routed(Vcpu_id vcpu, bool state) {
            if (routed[vcpu] == state)
                return;
            routed[vcpu] = state;
            dirty = true;
        }
    };
}

class Model::IrqController : public Vbus::Device {
public:
    explicit IrqController(const char *name) : Vbus::Device(name, IRQ_CONTROLLER) {}
    ~IrqController() override {}

    virtual bool config_irq(Vcpu_id, uint32 irq_id, bool hw, uint16 pintid, bool edge) = 0;
    virtual bool config_spi(uint32 irq_id, bool hw, uint16 pintid, bool edge) = 0;
    virtual bool assert_ppi(Vcpu_id, uint32) = 0;
    virtual void assert_msi(uint64 address, uint32 data, uint32 rid, IrqAssertionRecord *record = nullptr) = 0;
    virtual void deassert_line_ppi(Vcpu_id, uint32) = 0;
    virtual void enable_cpu(CpuIrqInterface *, Vcpu_id) = 0;
    virtual void disable_cpu(Vcpu_id id) = 0;

    virtual void deassert_global_line(uint32) = 0;
    virtual bool assert_global_line(uint32) = 0;

    virtual bool signal_eoi(uint8 vector) = 0;
    virtual bool wait_for_eoi(uint8 line) = 0;
};

class Model::LocalIrqController : public Vbus::Device {
public:
    explicit LocalIrqController(const char *name) : Vbus::Device(name, IRQ_CONTROLLER) {}
    ~LocalIrqController() override {}

    virtual bool can_receive_irq() const = 0;

    virtual void assert_vector(uint8 vec, bool edge) = 0;
    virtual uint8 int_ack() = 0;

    /* highest_irr is only used on x86 for TPR shadow to setup a threshold */
    virtual bool int_pending(uint8 *highest_irr) = 0;

    virtual void nmi_ack() = 0;
    virtual bool nmi_pending() = 0;
};
