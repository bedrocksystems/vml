/*
 * Copyright (C) 2020 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
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
// NOLINTBEGIN(readability-identifier-naming)

#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <platform/compiler.hpp>
#include <platform/errno.hpp>

namespace Log {
    enum LogLevel {
        DEBUG,
        VERBOSE,
        INFO,
        WARN,
        ERROR,
        FATAL,
    };

    __attribute__((format(printf, 3, 4))) inline void log(LogLevel, bool enabled, const char *fmt, ...) {
        if (!enabled)
            return;

        va_list args;
        va_start(args, fmt);
        vfprintf(stdout, fmt, args);
        va_end(args);
    }

    __attribute__((format(printf, 3, 0))) inline void vlog(LogLevel, bool enabled, const char *fmt, va_list vap) {
        if (!enabled)
            return;

        vfprintf(stdout, fmt, vap);
    }
}

#define FMTx64 "0x%" PRIx64
#define FMTu64 "%" PRIu64
#define FMTd64 "%" PRId64
#define FMTx32 "0x%" PRIx32
#define FMTu32 "%" PRIu32
#define FMTd32 "%" PRId32

#define _LOG(_FILE_STREAM_, _LVL_STR_, _FMT_, ...)                                                                               \
    fprintf(_FILE_STREAM_, "[%s][%s:%u] " _FMT_ "\n", _LVL_STR_, __FILE_NAME__, __LINE__, ##__VA_ARGS__);
#define DEBUG(_FMT_, ...) _LOG(stdout, "DBG", _FMT_, ##__VA_ARGS__)
#define VERBOSE(_FMT_, ...) _LOG(stdout, "VRB", _FMT_, ##__VA_ARGS__)
#define INFO(_FMT_, ...) _LOG(stdout, "INF", _FMT_, ##__VA_ARGS__)
#define WARN(_FMT_, ...) _LOG(stdout, "WRN", _FMT_, ##__VA_ARGS__)
#define ERROR(_FMT_, ...) _LOG(stderr, "ERR", _FMT_, ##__VA_ARGS__)
#define FATAL(_FMT_, ...) _LOG(stderr, "FTL", _FMT_, ##__VA_ARGS__)

#define ASSERT(_expr_) assert(_expr_)

/* SYSTEM: Use this for messages that necessarily always display. */
#define SYSTEM(_FMT_, ...) _LOG(stdout, "SYS", _FMT_, ##__VA_ARGS__)

#define ABORT_WITH(_FMT_, ...)                                                                                                   \
    do {                                                                                                                         \
        FATAL("ABORT_WITH: " _FMT_, ##__VA_ARGS__);                                                                              \
        cxx::abort();                                                                                                            \
    } while (0)

// Derived macros. Keep in sync with zeta/lib/log/include/bedrock/log/log.hpp
// TODO: dedup

// DEBUG will add the file and line number, emulating a poor man's stacktrace.
// We also print the expression that lead to the failure.
#define TRY_ERRNO_LOG(_expr_)                                                                                                    \
    do {                                                                                                                         \
        Errno ___err = _expr_;                                                                                                   \
        if (__UNLIKELY__(___err != Errno::NONE)) {                                                                               \
            DEBUG("Expression failed with %s: `%s`", errno2str(___err), #_expr_);                                                \
            return ___err;                                                                                                       \
        }                                                                                                                        \
    } while (0)

// Alias for TRY_ERRNO_LOG, which will be renamed in the future
#define TRY_ERRNO_DBG(_expr_) TRY_ERRNO_LOG(_expr_)

// Legacy macro.
#define PROPAGATE_ERRNO_FAILURE(_expr_) TRY_ERRNO_LOG(_expr_)

// Evaluate the result of [expr] which should be of type [Errno].
// If not [NONE] then print an ERROR message and return with that value from the current scope.
#define TRY_ERRNO_ERR(_expr_)                                                                                                    \
    do {                                                                                                                         \
        Errno ___err = _expr_;                                                                                                   \
        if (__UNLIKELY__(___err != Errno::NONE)) {                                                                               \
            ERROR("Expression '%s' failed: %s", #_expr_, errno2str(___err));                                                     \
            return ___err;                                                                                                       \
        }                                                                                                                        \
    } while (0)

// Evaluate the result of [expr] which should be of type [Errno].
// If not [NONE] then print a custom ERROR message and return with that value from the current scope.
#define TRY_ERRNO_ERR_MSG(_expr_, _fmt_, ...)                                                                                    \
    do {                                                                                                                         \
        Errno ___err = _expr_;                                                                                                   \
        if (__UNLIKELY__(___err != Errno::NONE)) {                                                                               \
            ERROR(_fmt_, ##__VA_ARGS__);                                                                                         \
            return ___err;                                                                                                       \
        }                                                                                                                        \
    } while (0)

// Evaluate the result of [expr] which should be of type [pointer].
// If not [nullptr] then return with that value from the current scope.
// Otherwise ABORT_WITH an error message.
#define TRY_PTR_ABORT(_expr_)                                                                                                    \
    ({                                                                                                                           \
        auto *___ptr = _expr_;                                                                                                   \
        if (___ptr == nullptr) {                                                                                                 \
            ABORT_WITH("Could not allocate memory!");                                                                            \
        }                                                                                                                        \
        ___ptr;                                                                                                                  \
    })

// Evaluate the result of [expr] which should be of type [Result].
// If not [Result.is_err()] then return with the value of the [Result] from the current scope.
// Otherwise print an ERROR and return from the function with [Errno].
#define TRY_RESULT_ERR(_expr_)                                                                                                   \
    ({                                                                                                                           \
        auto ___res = _expr_;                                                                                                    \
        if (___res.is_err()) {                                                                                                   \
            ERROR("Expression '%s' failed: %s", #_expr_, errno2str(___res.take_err()));                                          \
            return ___res.take_err();                                                                                            \
        }                                                                                                                        \
        ___res.take();                                                                                                           \
    })

// Evaluate the result of [expr] which should be of type [Result].
// If not [Result.is_err()] then return the value of the [Result] from the current scope.
// Otherwise print a custom ERROR message and return from the function with [Errno].
#define TRY_RESULT_ERR_MSG(_expr_, _fmt_, ...)                                                                                   \
    ({                                                                                                                           \
        auto ___res = _expr_;                                                                                                    \
        if (___res.is_err()) {                                                                                                   \
            ERROR(_fmt_, ##__VA_ARGS__);                                                                                         \
            return ___res.take_err();                                                                                            \
        }                                                                                                                        \
        ___res.take();                                                                                                           \
    })

// Evaluate the result of [expr] which should be of type [Result].
// If not [Result.is_err()] then return the value of the [Result] from the current scope.
// Otherwise ABORT_WITH a custom message.
#define TRY_RESULT_ABORT_MSG(_expr_, _fmt_, ...)                                                                                 \
    ({                                                                                                                           \
        auto ___res = _expr_;                                                                                                    \
        if (___res.is_err()) {                                                                                                   \
            ABORT_WITH(_fmt_, ##__VA_ARGS__);                                                                                    \
        }                                                                                                                        \
        ___res.take();                                                                                                           \
    })

// Variant of TRY_ERRNO_LOG that does not return.
// Useful for cleanup methods, since they should continue executing past a
// failure.
#define TRY_ERRNO_LOG_CONTINUE(_expr_)                                                                                           \
    do {                                                                                                                         \
        Errno ___err = _expr_;                                                                                                   \
        if (__UNLIKELY__(___err != Errno::NONE)) {                                                                               \
            DEBUG("Expression failed with %s: `%s`", errno2str(___err), #_expr_);                                                \
        }                                                                                                                        \
    } while (0)

// NOLINTEND(readability-identifier-naming)
