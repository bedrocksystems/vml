/*
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <mutex>
#include <platform/context.hpp>
#include <platform/errno.hpp>

// NOLINTBEGIN(readability-convert-member-functions-to-static)
/*! \file Define a mutex class
 */

namespace Platform {
    class Mutex;
    class MutexGuard;
}

class Platform::Mutex : public std::mutex {
public:
    bool init([[maybe_unused]] const Platform_ctx* ctx = nullptr) { return true; }

    Errno create(const Platform_ctx*) { return Errno::NONE; }

    Errno destroy(const Platform_ctx*) { return Errno::NONE; }

    bool enter() {
        lock();
        return true;
    }
    bool exit() {
        unlock();
        return true;
    }
};

class Platform::MutexGuard {
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
