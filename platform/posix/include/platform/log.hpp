/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file
 *  \brief Logging mechanism exposed by the platform
 *
 *  We expect the following macros to be exposed:
 *  - #define DEBUG(_fmt_, ...)
 *  - #define VERBOSE(_fmt_, ...)
 *  - #define INFO(_fmt_, ...)
 *  - #define WARN(_fmt_, ...)
 *  - #define ERROR(_fmt_, ...)
 *  - #define FATAL(_fmt_, ...)
 *  - #define ASSERT(_expr_)
 *  - #define ABORT_WITH(_msg_, ...)
 */

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

#include <platform/compiler.hpp>

#define FMTx64 "0x%" PRIx64
#define FMTu64 "%" PRIu64
#define FMTd64 "%" PRId64
#define FMTx32 "0x%" PRIx32
#define FMTu32 "%" PRIu32
#define FMTd32 "%" PRId32

#define _LOG(_LVL_STR_, _FMT_, ...)                                                                \
    printf("[%s][%s:%u] " _FMT_ "\n", _LVL_STR_, __FILE__, __LINE__, ##__VA_ARGS__);
#define DEBUG(_FMT_, ...) _LOG("DBG", _FMT_, ##__VA_ARGS__)
#define VERBOSE(_FMT_, ...) _LOG("VRB", _FMT_, ##__VA_ARGS__)
#define INFO(_FMT_, ...) _LOG("INF", _FMT_, ##__VA_ARGS__)
#define WARN(_FMT_, ...) _LOG("WRN", _FMT_, ##__VA_ARGS__)
#define ERROR(_FMT_, ...) _LOG("ERR", _FMT_, ##__VA_ARGS__)
#define FATAL(_FMT_, ...) _LOG("FTL", _FMT_, ##__VA_ARGS__)

#define ASSERT(_expr_)                                                                             \
    do {                                                                                           \
        if (__UNLIKELY__(!(_expr_))) {                                                             \
            FATAL("Assertion failure.");                                                           \
            assert(false);                                                                         \
        }                                                                                          \
    } while (0);

#define ABORT_WITH(_FMT_, ...)                                                                     \
    do {                                                                                           \
        FATAL(_FMT_, ##__VA_ARGS__);                                                               \
        exit(1);                                                                                   \
    } while (0);
