/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file Define a signal class
 */

#include <condition_variable>
#include <mutex>
#include <platform/context.hpp>
#include <platform/errno.hpp>

namespace Platform {
    class Signal;
}

/*! \brief A signal class implemented by the platform.
 *
 *  This class should provide at least:
 *  - An init function that take a Platform context
 *  - A wait function
 *  - A sig function
 */
class Platform::Signal {
public:
    Signal() : _mutex(new std::mutex()), _cv(new std::condition_variable()) {}

    ~Signal() {
        delete _mutex;
        delete _cv;
    }

    /*! \brief Initialize the signal
     *  \param ctx The platform-specific context
     *  \return true - no failure possible at the moment
     */
    bool init(const Platform_ctx *) {
        _valid = true;
        return true;
    }

    Errno create() {
        _valid = true;
        return Errno::NONE;
    }

    Errno create(const Platform_ctx *) {
        _valid = true;
        return Errno::NONE;
    }

    void destroy() { _valid = false; }

    // To stay compatible with zeta, we can accept a ctx
    Errno destroy(const Platform_ctx *) {
        destroy();
        return Errno::NONE;
    }

    /*! \brief Wait for a signal
     */
    void wait() {
        std::unique_lock<std::mutex> lock(*_mutex);
        while (!_signaled)
            _cv->wait(lock);

        _signaled = false;
    }

    bool wait(unsigned long long abs_ticks) {
        std::unique_lock<std::mutex> lock(*_mutex);

        while (!_signaled) {
            std::chrono::steady_clock::duration end(abs_ticks);
            std::chrono::steady_clock::time_point deadline(end);
            std::cv_status status;

            status = _cv->wait_until(lock, deadline);
            if (status == std::cv_status::timeout)
                return false;
        }

        _signaled = false;

        return true;
    }

    /*! \brief Signal a (future) waiter
     */
    void sig() {
        std::lock_guard<std::mutex> lock(*_mutex);
        if (_signaled)
            return;
        _signaled = true;
        _cv->notify_one();
    }

    bool is_valid() const { return _valid; }

private:
    std::mutex *_mutex;
    std::condition_variable *_cv;
    bool _signaled{false};
    bool _valid{false};
};
