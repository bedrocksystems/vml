/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
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