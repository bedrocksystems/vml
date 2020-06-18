/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Cpu_irq_interface;
    class Irq_controller;
}

class Model::Irq_controller : public Vbus::Device {
public:
    Irq_controller(const char *name) : Vbus::Device(name) {}
    virtual ~Irq_controller() {}

    virtual Vbus::Err access(Vbus::Access, const Vcpu_ctx *, mword, uint8, uint64 &) = 0;
    virtual void reset() = 0;

    virtual bool config_irq(Vcpu_id, uint32 irq_id, bool hw, uint16 pintid, bool edge) = 0;
    virtual bool config_spi(uint32 irq_id, bool hw, uint16 pintid, bool edge) = 0;
    virtual bool assert_ppi(Vcpu_id, uint32) = 0;
    virtual bool assert_spi(uint32) = 0;
    virtual void deassert_line_ppi(Vcpu_id, uint32) = 0;
    virtual void deassert_line_spi(uint32) = 0;
    virtual void enable_cpu(Cpu_irq_interface *, Vcpu_id const) = 0;
};