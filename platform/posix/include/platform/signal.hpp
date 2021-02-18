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
    Signal() : _mutex(), _cv(), _signaled(false) {}

    /*! \brief Initialize the signal
     *  \param ctx The platform-specific context
     *  \return true - no failure possible at the moment
     */
    bool init(const Platform_ctx*) { return true; }

    /*! \brief Wait for a signal
     */
    void wait() {
        std::unique_lock<decltype(_mutex)> lock(_mutex);
        while (!_signaled)
            _cv.wait(lock);

        _signaled = false;
    }

    /*! \brief Signal a (future) waiter
     */
    void sig() {
        std::lock_guard<decltype(_mutex)> lock(_mutex);
        if (_signaled)
            return;
        _signaled = true;
        _cv.notify_one();
    }

private:
    std::mutex _mutex;
    std::condition_variable _cv;
    bool _signaled;
};