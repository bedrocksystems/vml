/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
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
#define __UNREACHED__ __builtin_unreachable()

static inline int
ffs(unsigned int val) {
    return __builtin_ffs(static_cast<int>(val));
}
