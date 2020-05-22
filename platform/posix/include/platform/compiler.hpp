/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

/*! \file
 *  \brief Wrapper around Compiler provided builtins
 *
 *  We expect the following definitions:
 *  - __UNLIKELY__
 *  - __LIKELY__
 *  - ffs
 */

#define __LIKELY__(x) __builtin_expect(!!(x), 1)
#define __UNLIKELY__(x) __builtin_expect(!!(x), 0)

static inline int
ffs(unsigned int val) {
    return __builtin_ffs(static_cast<int>(val));
}