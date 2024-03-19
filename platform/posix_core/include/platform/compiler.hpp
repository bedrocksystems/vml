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

#include <stdlib.h>

#define __LIKELY__(x) __builtin_expect(!!(x), 1)
#define __UNLIKELY__(x) __builtin_expect(!!(x), 0)
#define __UNREACHED__ __builtin_unreachable()

#define ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

static inline int
ffs(unsigned int val) {
    return __builtin_ffs(static_cast<int>(val));
}

/* provide interception for ABORT_WITH in log.hpp */
extern void __on_abort();

namespace cxx {
    [[noreturn]] inline void abort() {
        __on_abort();
        exit(1);
    }
}
