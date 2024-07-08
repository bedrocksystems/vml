/*
 * Copyright (C) 2022 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

/*! \file Define a rwlock class
 */

#include <cassert>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <platform/atomic.hpp>
#include <platform/mutex.hpp>
#include <platform/types.hpp>

// NOLINTBEGIN(readability-convert-member-functions-to-static)
namespace Platform {
    class RWLock;
    class SharedLock;
    class UniqueLock;

    using std::defer_lock;
    using DeferLockT = std::defer_lock_t;
}

class Platform::RWLock {
public:
    RWLock() : _rq(), _wq(), _m(), _rw(0), _rwlock_signal(false) {}
    RWLock(RWLock&&) {}

    // Uncopyable
    RWLock(const RWLock&) = delete;
    RWLock(RWLock&) = delete;
    RWLock(const RWLock&&) = delete;

    Errno create(const Platform_ctx*) { return Errno::NONE; }
    Errno destroy(const Platform_ctx*) { return Errno::NONE; }
    bool init([[maybe_unused]] const Platform_ctx* = nullptr) { return true; }

    void wenter() {
        _rq.enter();
        if (((_rw |= 1) >> 1) != 0) {
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

class Platform::SharedLock {
public:
    SharedLock(void) = delete;
    explicit SharedLock(Platform::RWLock& rwlock) : _rwlock(&rwlock), _owns_mutex(false) { lock(); }
    SharedLock(Platform::RWLock& rwlock, Platform::DeferLockT) : _rwlock(&rwlock), _owns_mutex(false) {}

    SharedLock(const SharedLock&) = delete;
    SharedLock(SharedLock&& other) : _rwlock(other._rwlock), _owns_mutex(other._owns_mutex) {
        other._rwlock = nullptr;
        other._owns_mutex = false;
    }

    ~SharedLock(void) { unlock(); }

    SharedLock& operator=(const SharedLock& other) = delete;
    SharedLock& operator=(SharedLock&& other) {
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
        lock_shared();
    }

    void unlock(void) {
        if (owns_lock()) {
            unlock_shared();
        }
    }

    bool owns_lock(void) const { return _owns_mutex; }

private:
    void lock_shared(void) {
        assert(not owns_lock());
        _rwlock->renter();
        _owns_mutex = true;
    }

    void unlock_shared(void) {
        assert(owns_lock());
        _rwlock->rexit();
        _owns_mutex = false;
    }

private:
    Platform::RWLock* _rwlock;
    bool _owns_mutex;
};

class Platform::UniqueLock {
public:
    UniqueLock(void) = delete;
    explicit UniqueLock(Platform::RWLock& rwlock) : _rwlock(&rwlock), _owns_mutex(false) { lock(); }
    UniqueLock(Platform::RWLock& rwlock, Platform::DeferLockT) : _rwlock(&rwlock), _owns_mutex(false) {}

    UniqueLock(const UniqueLock&) = delete;
    UniqueLock(UniqueLock&& other) : _rwlock(other._rwlock), _owns_mutex(other._owns_mutex) {
        other._rwlock = nullptr;
        other._owns_mutex = false;
    }

    ~UniqueLock(void) { unlock(); }

    UniqueLock& operator=(const UniqueLock&) = delete;
    UniqueLock& operator=(UniqueLock&& other) {
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
        do_lock();
    }

    void unlock(void) {
        if (owns_lock()) {
            do_unlock();
        }
    }

    bool owns_lock(void) const { return _owns_mutex; }

private:
    void do_lock(void) {
        assert(not owns_lock());
        _rwlock->wenter();
        _owns_mutex = true;
    }

    void do_unlock(void) {
        assert(owns_lock());
        _rwlock->wexit();
        _owns_mutex = false;
    }

private:
    Platform::RWLock* _rwlock;
    bool _owns_mutex;
};
// NOLINTEND(readability-convert-member-functions-to-static)
