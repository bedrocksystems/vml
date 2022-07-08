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
    enum Status {
        OK,
        ERROR,
        WFI,
        /* Future: deeper power saving states could be added */
    };

    Status smc_call_service(const VcpuCtx &vctx, RegAccessor &arch, Vbus::Bus &vbus,
                            uint64 function_id, uint64 &res);
}
