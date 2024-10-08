/**
 * Copyright (C) 2020 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/context.hpp>
#include <platform/errno.hpp>
#include <platform/types.hpp>

namespace Vcpu::Roundup {
    Errno init(const Platform_ctx* ctx, uint16 num_vcpus);
    Errno cleanup(const Platform_ctx* ctx);

    void roundup();
    void resume();

    void roundup_from_vcpu(Vcpu_id vcpu_id);
    void resume_from_vcpu(Vcpu_id vcpu_id);

    void roundup_parallel(Vcpu_id vcpu_id);
    void resume_parallel(Vcpu_id vcpu_id);

    void wait_for_all_off();
    void vcpu_notify_initialized();

    void vcpu_notify_done_progressing();

}
