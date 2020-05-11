/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
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