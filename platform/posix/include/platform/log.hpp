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
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <platform/compiler.hpp>

namespace Log {
    enum Log_level {
        DEBUG,
        VERBOSE,
        INFO,
        WARN,
        ERROR,
        FATAL,
    };

    __attribute__((format(printf, 3, 4))) inline void _log(Log_level, bool enabled, const char *fmt,
                                                           ...) {
        if (!enabled)
            return;

        va_list args;
        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        va_end(args);
    }
}

#define FMTx64 "0x%" PRIx64
#define FMTu64 "%" PRIu64
#define FMTd64 "%" PRId64
#define FMTx32 "0x%" PRIx32
#define FMTu32 "%" PRIu32
#define FMTd32 "%" PRId32

#define _LOG(_FILE_STREAM_, _LVL_STR_, _FMT_, ...)                                                 \
    fprintf(_FILE_STREAM_, "[%s][%s:%u] " _FMT_ "\n", _LVL_STR_, __FILE__, __LINE__, ##__VA_ARGS__);
#define DEBUG(_FMT_, ...) _LOG(stdout, "DBG", _FMT_, ##__VA_ARGS__)
#define VERBOSE(_FMT_, ...) _LOG(stdout, "VRB", _FMT_, ##__VA_ARGS__)
#define INFO(_FMT_, ...) _LOG(stdout, "INF", _FMT_, ##__VA_ARGS__)
#define WARN(_FMT_, ...) _LOG(stdout, "WRN", _FMT_, ##__VA_ARGS__)
#define ERROR(_FMT_, ...) _LOG(stderr, "ERR", _FMT_, ##__VA_ARGS__)
#define FATAL(_FMT_, ...) _LOG(stderr, "FTL", _FMT_, ##__VA_ARGS__)

#define ASSERT(_expr_)                                                                             \
    do {                                                                                           \
        if (__UNLIKELY__(!(_expr_))) {                                                             \
            FATAL("Assertion failure.");                                                           \
            cxx::abort();                                                                          \
        }                                                                                          \
    } while (0)

#define ABORT_WITH(_FMT_, ...)                                                                     \
    do {                                                                                           \
        FATAL(_FMT_, ##__VA_ARGS__);                                                               \
        cxx::abort();                                                                              \
    } while (0)
