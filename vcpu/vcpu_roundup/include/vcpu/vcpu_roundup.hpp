/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/context.hpp>
#include <platform/types.hpp>

namespace Vcpu::Roundup {
    Errno init(const Platform_ctx* ctx, uint16 num_vcpus);
    void roundup();
    void roundup_from_vcpu(Vcpu_id vcpu_id);
    void resume();

    void wait_for_all_off();
    void vcpu_notify_switched_off();
    void vcpu_notify_switched_on();

    void vcpu_notify_done_progessing();
}