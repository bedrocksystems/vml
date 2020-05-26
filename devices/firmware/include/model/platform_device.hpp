/**
 * Copyright (c) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#pragma once

#include <types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Platform_device;
}

class Pm_client;

class Model::Platform_device : public Vbus::Device {
private:
    Pm_client *_plat_mgr;
    uint8 _reg_id;

public:
    Platform_device() : Vbus::Device(nullptr), _plat_mgr(nullptr) {}
    Platform_device(const char *name, Pm_client *plat_mgr, uint8 reg_id)
        : Vbus::Device(name), _plat_mgr(plat_mgr), _reg_id(reg_id) {}

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx *, mword off, uint8 bytes,
                             uint64 &res) override;

    virtual void reset() override {}
};
