/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/timer.hpp>
#include <platform/context.hpp>
#include <platform/log.hpp>

void
Model::Timer::timer_loop(const Platform_ctx*, Model::Timer* timer) {
    ASSERT(timer != nullptr);

    timer->set_ready();

    while (not timer->_terminate) {
        bool released;
        uint64 curr_timer;

        /*
         * Use fired to prevent asserting the interrupt several times. We then wait
         * for some timer register to change before firing again.
         */
        if (!timer->can_fire() || timer->is_irq_status_set()) {
            timer->set_wait_timeout(0);
            timer->timer_wait();
            released = true;
            timer->clear_irq_status();
        } else {
            uint64 timeout = timer->get_timeout_abs();
            timer->set_wait_timeout(timeout);

            // Wait for the timer to expire or to be cancelled(using timer_wakeup())
            released = timer->timer_wait_timeout(timeout);

            // After wakeup, deadline is updated?
            curr_timer = timer->get_timeout_abs();
            released = released && !timer->curr_timer_expired(curr_timer);
        }

        // false => timeout
        if (!released) {
            bool fired = timer->can_fire() && timer->assert_irq();
            if (fired) {
                timer->set_wait_timeout(curr_timer);
                timer->set_irq_status(true);
            }
        }
    }

    timer->set_terminated();
}

bool
Model::Timer::init_timer_loop(const Platform_ctx* ctx) {
    return _wait_timer.init(ctx) && _ready_sig.init(ctx) && _terminated_sig.init(ctx);
}

void
Model::Timer::terminate() {
    _terminate = true;
    timer_wakeup();
}

bool
Model::Timer::cleanup_timer_loop_resources(const Platform_ctx* ctx) {
    bool ret = true;

    ret &= Errno::NONE != _terminated_sig.destroy(ctx);
    ret &= Errno::NONE != _ready_sig.destroy(ctx);
    ret &= Errno::NONE != _wait_timer.destroy(ctx);

    return ret;
}
