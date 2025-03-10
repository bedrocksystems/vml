/*
 * Copyright (C) 2020-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#pragma once

/*! \file Interfaces to monitor and control the VM lifecycle.
 */

#include <model/vcpu_types.hpp>

namespace Lifecycle {
    void notify_system_reset(const VcpuCtx &) {
    }
    void notify_system_off(const VcpuCtx &) {
    }

    /* Those intefaces are not yet implemented */
    bool stop_vm() {
        return false;
    }
    bool reset_vm() {
        return false;
    }

    void start_system() {
    }
    void stop_system(const VcpuCtx &) {
    }

    bool can_shutdown_system() {
        return true;
    }
}
