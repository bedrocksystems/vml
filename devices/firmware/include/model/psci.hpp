/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
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