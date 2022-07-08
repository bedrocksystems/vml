/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file
 *  \brief Types that are specific to the VM logic
 *
 *  For now, we expect:
 *  - Reg_selection: a bitfield allowing to select registers
 */

#include <cstdint>

using Reg_selection = uint64_t;
