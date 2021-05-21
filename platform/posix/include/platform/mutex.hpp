/*
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <mutex>

/*! \file Define a mutex class
 */

namespace Platform {
    class Mutex;
    class MutexGuard;
}

class Platform::Mutex : public std::mutex {
public:
    bool init(const Platform_ctx*) { return true; }

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
    explicit MutexGuard(Platform::Mutex* m) : _m(m) { _m->enter(); }
    ~MutexGuard() { _m->exit(); }

private:
    Platform::Mutex* _m;
};
