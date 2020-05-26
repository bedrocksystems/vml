/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

/*! \brief Wrapper around the time management functions in zeta
 */

#include <platform.hpp>

using clock_t = Tsc;

static inline clock_t
clock() {
    return tsc();
}