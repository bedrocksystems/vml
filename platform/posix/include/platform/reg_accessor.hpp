/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/context.hpp>
#include <platform/vm_types.hpp>

class Reg_accessor {
public:
    Reg_accessor(const Platform_ctx&, const Reg_selection) {}

    inline uint64 gpr(uint8) const { return 0; }
    inline void gpr(uint8, const uint64, bool overwrite = false) { (void)overwrite; }

    inline uint64 tmr_cntvoff() const { return 0; }
    inline uint64 el1_sctlr() const { return 0; }
    inline uint64 el2_spsr() const { return 0; }
};
