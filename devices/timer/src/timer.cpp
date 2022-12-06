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

    INFO("The physical timer is ready");
    timer->set_ready();

    while (not timer->_terminate) {
        bool released;

        /*
         * Use fired to prevent asserting the interrupt several times. We then wait
         * for some timer register to change before firing again.
         */
        if (!timer->can_fire() || timer->is_irq_status_set()) {
            timer->timer_wait();
            released = true;
            timer->clear_irq_status();
        } else {
            released = timer->timer_wait_timeout(timer->get_timeout_abs());
        }

        // false => timeout
        if (!released) {
            bool fired = timer->can_fire() && timer->assert_irq();
            if (fired)
                timer->set_irq_status(true);
        }
    }
}

bool
Model::Timer::init_timer_loop(const Platform_ctx* ctx) {
    return _wait_timer.init(ctx) && _ready_sig.init(ctx);
}

void
Model::Timer::terminate() {
    _terminate = true;
    timer_wakeup();
}
