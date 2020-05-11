/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*! \file Define a semaphore class
 */

#include <platform/context.hpp>
#include <zeta/semaphore.hpp>

class Semaphore : private Zeta::Semaphore {
public:
    Semaphore() : Zeta::Semaphore() {}

    bool init(const Platform_ctx* ctx) { return Errno::ENONE == create(ctx, 0); }

    void acquire() { Zeta::Semaphore::acquire(); }
    bool try_acquire_until(unsigned long long ticks) {
        return Zeta::Semaphore::try_acquire_until(ticks);
    }
    void release() { Zeta::Semaphore::release(); }

    void init(Sel sm) {
        _sel = sm;
        _count = 0;
    }
};