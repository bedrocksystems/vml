/**
 * Copyright (c) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <types.hpp>

namespace Model {
    class Firmware;
}

class Pm_client;

class Model::Firmware {
public:
    Firmware(Pm_client *plat_mgr) : _plat_mgr(plat_mgr) {}

    bool handle_smc(const Vcpu_ctx *vcpu_ctx, mword p0, mword p1, mword p2, mword p3, mword p4,
                    mword p5, mword p6, mword (&ret)[4]) const;

private:
    Pm_client *_plat_mgr;
};
