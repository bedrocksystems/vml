/*
 * Copyright (C) 2022 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#pragma once

#include <platform/log.hpp>

namespace VmmAbort {
    /*
     * Variants of [cxx::abort()] with different intention and spec.
     * For now, they are equivalent, but:
     * - They may have differents specifications.
     * - In the future, they may result in different behaviors (ignore, reboot, stop, ...)
     */
    [[noreturn]] inline void abort_undefined() {
        cxx::abort();
    }

    [[noreturn]] inline void abort_unexpected() {
        cxx::abort();
    }

    [[noreturn]] inline void abort_hw_not_supported() {
        cxx::abort();
    }

    [[noreturn]] inline void abort_bad_config() {
        cxx::abort();
    }

    [[noreturn]] inline void abort_not_supported() {
        cxx::abort();
    }
}

/*
 * Wrappers around abort that explain why we need to abort.
 */

#define ABORT_UNDEFINED(_BEHAVIOR_, _FMT_, ...)                                                                                  \
    do {                                                                                                                         \
        FATAL("Behavior '%s' is undefined. " _FMT_, _BEHAVIOR_, ##__VA_ARGS__);                                                  \
        VmmAbort::abort_undefined();                                                                                             \
    } while (0)

#define ABORT_UNEXPECTED(_BEHAVIOR_, _FMT_, ...)                                                                                 \
    do {                                                                                                                         \
        FATAL("'%s' is unexpected. " _FMT_, _BEHAVIOR_, ##__VA_ARGS__);                                                          \
        VmmAbort::abort_unexpected();                                                                                            \
    } while (0)

#define ABORT_HW_NOT_SUPPORTED(_FEATURE_NAME_, _FMT_, ...)                                                                       \
    do {                                                                                                                         \
        FATAL("'%s' is not supported by the hardware. " _FMT_, _FEATURE_NAME_, ##__VA_ARGS__);                                   \
        VmmAbort::abort_hw_not_supported();                                                                                      \
    } while (0)

#define ABORT_BAD_CONFIG(_FEATURE_NAME_, _FMT_, ...)                                                                             \
    do {                                                                                                                         \
        FATAL("'%s' is not a valid configuration. " _FMT_, _FEATURE_NAME_, ##__VA_ARGS__);                                       \
        VmmAbort::abort_bad_config();                                                                                            \
    } while (0)

#define ABORT_NOT_SUPPORTED(_FEATURE_NAME_, _FMT_, ...)                                                                          \
    do {                                                                                                                         \
        FATAL("Feature '%s' is not supported. " _FMT_, _FEATURE_NAME_, ##__VA_ARGS__);                                           \
        VmmAbort::abort_not_supported();                                                                                         \
    } while (0)
