/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Cpu_irq_interface;
    class Irq_controller;
    class Local_Irq_controller;

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

    enum GICVersion {
        GIC_UNKNOWN = 0,
        GIC_V2 = 2,
        GIC_V3 = 3,
    };
}

class Model::Irq_controller : public Vbus::Device {
public:
    explicit Irq_controller(const char *name) : Vbus::Device(name) {}
    virtual ~Irq_controller() {}

    virtual bool config_irq(Vcpu_id, uint32 irq_id, bool hw, uint16 pintid, bool edge,
                            Platform::Signal *hw_enable_sig)
        = 0;
    virtual bool config_spi(uint32 irq_id, bool hw, uint16 pintid, bool edge,
                            Platform::Signal *hw_enable_sig)
        = 0;
    virtual bool assert_ppi(Vcpu_id, uint32) = 0;
    virtual void deassert_line_ppi(Vcpu_id, uint32) = 0;
    virtual void enable_cpu(Cpu_irq_interface *, Vcpu_id) = 0;

    virtual void deassert_global_line(uint32) = 0;
    virtual bool assert_global_line(uint32) = 0;

    virtual bool signal_eoi(uint8 vector) = 0;
    virtual bool wait_for_eoi(uint8 line) = 0;
};

class Model::Local_Irq_controller : public Vbus::Device {
public:
    explicit Local_Irq_controller(const char *name) : Vbus::Device(name) {}
    virtual ~Local_Irq_controller() {}

    virtual bool can_receive_irq() const = 0;

    virtual void assert_vector(uint8 vec, bool edge) = 0;
    virtual uint8 int_ack() = 0;
    virtual bool int_pending() = 0;

    virtual void nmi_ack() = 0;
    virtual bool nmi_pending() = 0;
};
