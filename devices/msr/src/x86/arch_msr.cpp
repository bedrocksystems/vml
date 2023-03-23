/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <msr/arch_msr.hpp>
#include <msr/msr_info.hpp>

// Stubs for ARM-specific functionality.
Msr::Err
Msr::SctlrEl1::access(Vbus::Access access, const VcpuCtx *, uint64 &) {
    ASSERT(access == Vbus::Access::WRITE); // We only trap writes at the moment

    return Msr::Err::UPDATE_REGISTER; // Tell the VCPU to update the relevant physical
                                      // register
}

bool
Msr::Bus::setup_aarch64_physical_timer(Model::AA64Timer &) { // NOLINT(readability-convert-member-functions-to-static)
    return true;
}
