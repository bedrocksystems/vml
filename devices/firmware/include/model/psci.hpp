/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/vcpu_types.hpp>
#include <platform/types.hpp>

namespace Vbus {
    class Bus;
}

namespace Firmware::Psci {
    bool smc_call_service(const Vcpu_ctx &vctx, Reg_accessor &arch, Vbus::Bus &vbus,
                          uint64 const function_id, uint64 &res);
}
