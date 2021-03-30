/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Cpu_irq_interface;
    class Irq_controller;
    class Local_Irq_controller;
}

class Model::Irq_controller : public Vbus::Device {
public:
    Irq_controller(const char *name) : Vbus::Device(name) {}
    virtual ~Irq_controller() {}

    virtual bool config_irq(Vcpu_id, uint32 irq_id, bool hw, uint16 pintid, bool edge) = 0;
    virtual bool config_spi(uint32 irq_id, bool hw, uint16 pintid, bool edge) = 0;
    virtual bool assert_ppi(Vcpu_id, uint32) = 0;
    virtual bool assert_spi(uint32) = 0;
    virtual void deassert_line_ppi(Vcpu_id, uint32) = 0;
    virtual void deassert_line_spi(uint32) = 0;
    virtual void enable_cpu(Cpu_irq_interface *, Vcpu_id const) = 0;
};

class Model::Local_Irq_controller : public Vbus::Device {
public:
    Local_Irq_controller(const char *name) : Vbus::Device(name) {}
    virtual ~Local_Irq_controller() {}

    virtual uint8 aff0() const = 0;
    virtual uint8 aff1() const = 0;
    virtual uint8 aff2() const = 0;
    virtual uint8 aff3() const = 0;

    virtual bool can_receive_irq() const = 0;
};
