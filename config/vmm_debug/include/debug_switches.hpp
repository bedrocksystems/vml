/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace Debug {
    static constexpr bool SANITY_CHECK_VM_EXIT_RESUME = false;
    static constexpr bool TRACE_SYSTEM_REGS = false;
    static constexpr bool TRACE_VBUS = false;
    static constexpr bool TRACE_SMC = false;
    static constexpr bool TRACE_INTR_INJECTION = false;
    static constexpr bool TRACE_INTR_ROUTING = false;
    static constexpr bool TRACE_INTR_SGI = false;
    static constexpr bool TRACE_VCPU_STATE_TRANSITION = false;
    static constexpr bool GUEST_MAP_ON_DEMAND = false;
    static constexpr bool TRACE_PAGE_PERMISSIONS = false;
};