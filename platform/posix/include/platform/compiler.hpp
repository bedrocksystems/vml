/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/*! \file
 *  \brief Wrapper around Compiler provided builtins
 *
 *  We expect the following definitions:
 *  - __UNLIKELY__
 *  - ffs
 */

#define __UNLIKELY__(x) __builtin_expect(!!(x), 0)

static inline int
ffs(unsigned int val) {
    return __builtin_ffs(static_cast<int>(val));
}