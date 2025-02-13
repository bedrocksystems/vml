/*
 * Copyright (C) 2021-2025 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <mutex>
#include <platform/compiler.hpp>
#include <platform/context.hpp>
#include <platform/errno.hpp>

// NOLINTBEGIN(readability-convert-member-functions-to-static)
/*! \file Define a mutex class
 */

namespace Platform {
    class Mutex;
    class MutexGuard;
}

class CAPABILITY("mutex") Platform::Mutex : public std::mutex {
public:
    bool init([[maybe_unused]] const Platform_ctx* ctx = nullptr) { return true; }

    Errno create(const Platform_ctx*) { return Errno::NONE; }

    Errno destroy(const Platform_ctx*) { return Errno::NONE; }

    bool enter() ACQUIRE() {
        lock();
        return true;
    }
    bool exit() RELEASE() {
        unlock();
        return true;
    }
};

class SCOPED_CAPABILITY Platform::MutexGuard {
public:
    explicit MutexGuard(Platform::Mutex& m) : _m(m) { _m.enter(); }
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;

    MutexGuard(MutexGuard&&) = delete;
    MutexGuard& operator=(MutexGuard&&) = delete;
    ~MutexGuard() { _m.exit(); }

private:
    Platform::Mutex& _m;
};

// NOLINTEND(readability-convert-member-functions-to-static)
