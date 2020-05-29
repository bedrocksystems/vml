/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#pragma once

#include <arch/breakpoint.hpp>
#include <platform/errno.hpp>
#include <vmi_interface/vmi_interface.hpp>

/*
 * This is the library that is used by the VMM when VMI is not enabled.
 * We provide dummy functions with the same signatures and the same spec
 * as the outpost library. We do nothing of interest here. This lib should
 * be updated when outpost is updated.
 */

namespace Zeta {
    class Zeta_ctx;
}

namespace Model {
    class Board;
}

struct Vcpu_ctx;
struct Uuid;

namespace outpost {
    inline void vmi_handle_page_fault(const Vcpu_ctx &, Vmm::Pf::Access_info &) {}
    inline void vmi_handle_recall(const Vcpu_ctx &) {}
    inline void vmi_handle_msr_update(const Vcpu_ctx &, Vmm::Msr::Trap_info &) {}
    inline void vmi_vcpu_startup(const Vcpu_ctx &) {}
    inline void vmi_handle_singlestep(const Vcpu_ctx &) {}
    inline void vmi_handle_singlestep_failure(const Vcpu_ctx &) {}
    inline bool vmi_handle_breakpoint(const Vcpu_ctx &, Breakpoint::Type, uint16) { return false; }
    inline Errno init(const Zeta::Zeta_ctx *, Uuid &, bool, Model::Board &) { return ENONE; }
}