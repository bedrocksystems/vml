/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

/*! \file Define a semaphore class
 */

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <platform/context.hpp>

/*! \brief A semaphore class implemented by the platform.
 *
 *  This class should provide at least:
 *  - An init function that take a Platform context
 *  - An acquire and acquire_until function
 *  - A release function
 */
class Semaphore {
public:
    Semaphore() : _mutex(), _cv(), _count(0) {}

    /*! \brief Initialize the semaphore
     *  \param ctx The platform-specific context
     *  \return true - no failure possible at the moment
     */
    bool init(const Platform_ctx*) { return true; }

    /*! \brief Acquire the semaphore - returns immediately if release was called
     */
    void acquire() {
        std::unique_lock<decltype(_mutex)> lock(_mutex);
        while (!_count)
            _cv.wait(lock);
        --_count;
    }

    /*! \brief Acquire the semaphore until the given time point is reached
     *  \param abs_ticks Absolute deadline in steady system ticks
     *  \return true if a call to release woke us up otherwise false (timeout case)
     */
    bool try_acquire_until(unsigned long long abs_ticks) {
        std::unique_lock<decltype(_mutex)> lock(_mutex);

        while (!_count) {
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
    unsigned long long _count;
};