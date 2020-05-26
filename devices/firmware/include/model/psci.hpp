/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#pragma once

#include <model/vcpu_types.hpp>
#include <platform/types.hpp>

namespace Vbus {
    class Bus;
}

namespace Firmware::Psci {
    bool smc_call_service(Vcpu_ctx &vctx, Vbus::Bus &vbus, uint64 const function_id, uint64 &res);
}