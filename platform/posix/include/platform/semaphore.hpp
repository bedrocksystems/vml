/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file Define a semaphore class
 */

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <platform/context.hpp>
#include <platform/errno.hpp>

/*! \brief A semaphore class implemented by the platform.
 *
 *  This class should provide at least:
 *  - An init function that take a Platform context
 *  - An acquire and acquire_until function
 *  - A release function
 */

namespace Platform {
    class Semaphore;
}

using Platform::Semaphore;

class Platform::Semaphore {
public:
    Semaphore() : _mutex(), _cv() {}

    /*! \brief Initialize the semaphore
     *  \param ctx The platform-specific context
     *  \return true - no failure possible at the moment
     */
    bool init([[maybe_unused]] const Platform_ctx* ctx = nullptr, size_t count = 0) {
        _count = count;
        return true;
    }

    Errno destroy([[maybe_unused]] const Platform_ctx* ctx = nullptr) {
        _count = 0;
        return Errno::NONE;
    }

    /*! \brief Acquire the semaphore - returns immediately if release was called
     */
    void acquire() {
        std::unique_lock<decltype(_mutex)> lock(_mutex);
        while (_count == 0)
            _cv.wait(lock);
        --_count;
    }

    /*! \brief Acquire the semaphore until the given time point is reached
     *  \param abs_ticks Absolute deadline in steady system ticks
     *  \return true if a call to release woke us up otherwise false (timeout case)
     */
    bool try_acquire_until(unsigned long long abs_ticks) {
        std::unique_lock<decltype(_mutex)> lock(_mutex);

        while (_count == 0) {
            std::chrono::steady_clock::duration end(abs_ticks);
            std::chrono::steady_clock::time_point deadline(end);
            std::cv_status status;

            status = _cv.wait_until(lock, deadline);
            if (status == std::cv_status::timeout)
                return false;
        }
        --_count;

        return true;
    }

    /*! \brief Release the semaphore (wake up one waiter if any)
     */
    void release() {
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        ++_count;
        _cv.notify_one();
    }

private:
    std::mutex _mutex;
    std::condition_variable _cv;
    unsigned long long _count{0};
};
