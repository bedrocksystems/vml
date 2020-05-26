/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <model/physical_timer.hpp>
#include <model/timer.hpp>
#include <platform/context.hpp>
#include <platform/log.hpp>

[[noreturn]] void
Model::Physical_timer::timer_loop(const Platform_ctx*, Model::Physical_timer* timer) {
    ASSERT(timer != nullptr);

    INFO("The physical timer is ready");
    timer->set_ready();

    while (1) {
        bool released;

        /*
         * Use fired to prevent asserting the interrupt several times. We then wait
         * for some timer register to change before firing again.
         */
        if (!timer->can_fire() || timer->is_istatus_set()) {
            timer->get_timer_sm().acquire();
            released = true;
            timer->clear_istatus();
        } else {
            released = timer->get_timer_sm().try_acquire_until(timer->get_cval());
        }

        // false => timeout
        if (!released) {
            bool fired = timer->assert_irq(timer->get_ctl());
            if (fired)
                timer->set_istatus();
        }
    }
}

bool
Model::Physical_timer::init(const Platform_ctx* ctx) {
    bool ok = _wait_timer.init(ctx);
    if (!ok)
        return false;

    ok = _ready_sm.init(ctx);
    if (!ok)
        return false;

    return true;
}