/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

/*! \file
 *  \brief Basic types exposed by the platform
 *
 *  We expect the following types to be exposed:
 *   - (u)int8
 *   - (u)int16
 *   - (u)int32
 *   - (u)int64
 *   - size_t
 *   - mword
 *   - Reg_selection
 */

#include <cstddef>
#include <cstdint>

using uint8 = uint8_t;
using int8 = int8_t;
using uint16 = uint16_t;
using int16 = int16_t;
using uint32 = uint32_t;
using int32 = int32_t;
using uint64 = uint64_t;
using int64 = int64_t;
using mword = unsigned long;
using Reg_selection = uint64_t;