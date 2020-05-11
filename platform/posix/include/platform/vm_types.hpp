/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
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