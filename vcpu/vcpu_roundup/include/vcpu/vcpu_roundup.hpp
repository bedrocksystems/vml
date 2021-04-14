/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/context.hpp>
#include <platform/types.hpp>

namespace Vcpu::Roundup {
    Errno init(const Platform_ctx* ctx, uint16 num_vcpus);
    void roundup();
    void resume();

    void roundup_from_vcpu(Vcpu_id vcpu_id);
    void resume_from_vcpu(Vcpu_id vcpu_id);

    void roundup_parallel(Vcpu_id vcpu_id);
    void resume_parallel(Vcpu_id vcpu_id);

    void wait_for_all_off();
    void vcpu_notify_initialized();
    void vcpu_notify_switched_on();

    void vcpu_notify_done_progressing();

}
