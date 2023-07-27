/**
 * Copyright (C) 2023 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once
#include <msr/msr_base.hpp>

// Temporarily empty
namespace Msr {
    class Bus;
};

class Msr::Bus : public Msr::BaseBus {
public:
    Bus() {}
};
