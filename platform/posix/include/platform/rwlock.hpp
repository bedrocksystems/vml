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
    class shared_lock;
    class unique_lock;

    using std::defer_lock;
    using std::defer_lock_t;
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

class Platform::shared_lock {
public:
    shared_lock(void) = delete;
    explicit shared_lock(Platform::RWLock& rwlock) : _rwlock(&rwlock), _owns_mutex(false) { lock(); }
    shared_lock(Platform::RWLock& rwlock, Platform::defer_lock_t) : _rwlock(&rwlock), _owns_mutex(false) {}

    shared_lock(const shared_lock&) = delete;
    shared_lock(shared_lock&& other) : _rwlock(other._rwlock), _owns_mutex(other._owns_mutex) {
        other._rwlock = nullptr;
        other._owns_mutex = false;
    }

    ~shared_lock(void) { unlock(); }

    shared_lock& operator=(const shared_lock& other) = delete;
    shared_lock& operator=(shared_lock&& other) {
        if (&other != this) {
            unlock();
            _rwlock = other._rwlock;
            _owns_mutex = other._owns_mutex;
            other._rwlock = nullptr;
            other._owns_mutex = false;
        }
        return *this;
    }

    void lock(void) {
        assert(not owns_lock());
        _lock_shared();
    }

    void unlock(void) {
        if (owns_lock()) {
            _unlock_shared();
        }
    }

    bool owns_lock(void) const { return _owns_mutex; }

private:
    void _lock_shared(void) {
        assert(not owns_lock());
        _rwlock->renter();
        _owns_mutex = true;
    }

    void _unlock_shared(void) {
        assert(owns_lock());
        _rwlock->rexit();
        _owns_mutex = false;
    }

private:
    Platform::RWLock* _rwlock;
    bool _owns_mutex;
};

class Platform::unique_lock {
public:
    unique_lock(void) = delete;
    explicit unique_lock(Platform::RWLock& rwlock) : _rwlock(&rwlock), _owns_mutex(false) { lock(); }
    unique_lock(Platform::RWLock& rwlock, Platform::defer_lock_t) : _rwlock(&rwlock), _owns_mutex(false) {}

    unique_lock(const unique_lock&) = delete;
    unique_lock(unique_lock&& other) : _rwlock(other._rwlock), _owns_mutex(other._owns_mutex) {
        other._rwlock = nullptr;
        other._owns_mutex = false;
    }

    ~unique_lock(void) { unlock(); }

    unique_lock& operator=(const unique_lock&) = delete;
    unique_lock& operator=(unique_lock&& other) {
        if (&other != this) {
            unlock();
            _rwlock = other._rwlock;
            _owns_mutex = other._owns_mutex;
            other._rwlock = nullptr;
            other._owns_mutex = false;
        }
        return *this;
    }

    void lock(void) {
        assert(not owns_lock());
        _lock();
    }

    void unlock(void) {
        if (owns_lock()) {
            _unlock();
        }
    }

    bool owns_lock(void) const { return _owns_mutex; }

private:
    void _lock(void) {
        assert(not owns_lock());
        _rwlock->wenter();
        _owns_mutex = true;
    }

    void _unlock(void) {
        assert(owns_lock());
        _rwlock->wexit();
        _owns_mutex = false;
    }

private:
    Platform::RWLock* _rwlock;
    bool _owns_mutex;
};
