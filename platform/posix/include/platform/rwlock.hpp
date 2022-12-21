/*
 * Copyright (C) 2022 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file Define a rwlock class
 */

#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <platform/atomic.hpp>
#include <platform/mutex.hpp>
#include <platform/types.hpp>

namespace Platform {
    class RWLock;
}

class Platform::RWLock {
public:
    RWLock() : _rq(), _wq(), _m(), _rw(0), _rwlock_signal(false) {}
    explicit RWLock(RWLock&&) {}

    // Uncopyable
    RWLock(const RWLock&) = delete;
    RWLock(RWLock&) = delete;
    RWLock(const RWLock&&) = delete;

    Errno create(const Platform_ctx*) { return Errno::NONE; }
    Errno destroy(const Platform_ctx*) { return Errno::NONE; }
    bool init([[maybe_unused]] const Platform_ctx* = nullptr) { return true; }

    void wenter() {
        _rq.enter();
        if ((_rw |= 1) >> 1) {
            {
                std::unique_lock<std::mutex> lock(_m);
                _wq.wait(lock, [this] { return _rwlock_signal.load(); });
            }

            _rwlock_signal = false;
        }
    }

    void renter() {
        _rq.enter();
        _rw += 2;
        _rq.exit();
    }

    void rexit() {
        if ((_rw -= 2) == 1) {
            std::lock_guard<std::mutex> lock(_m);
            _rwlock_signal = true;
            _wq.notify_all();
        }
    }

    void wexit() {
        _rw = 0;
        _rq.exit();
    }

private:
    Platform::Mutex _rq;

    // std::condition_variable
    std::condition_variable _wq;

    Platform::Mutex _m;

    atomic<size_t> _rw;

    atomic<bool> _rwlock_signal;
};
