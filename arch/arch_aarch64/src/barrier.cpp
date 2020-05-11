/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/barrier.hpp>

namespace Barrier {

    /*
     * Semantic of the functions below:
     * op1_before_op2
     * where op1/op2 can be r -> read (load), w -> write (store) or both.
     * op1 must be completed before entering the barrier. And, op2 must wait
     * before the barrier before completing.
     *
     * For example, r_before_rw ensures that all read operations are
     * completed before the barrier and all read and write operations
     * after the barrier must wait for the barrier to complete.
     *
     * Note that on ARM, there exists Instruction Synchronization Barriers
     * also that make sure that all instructions are completed before the
     * barrier. Those are typically used when dealing with memory management,
     * cache control, self-editing code or SVE/WFE. For now, I don't think
     * the VMM needs to deal with those cases. The kernel will issue an ISB
     * on context switching (before jumping back to the guest) and the VMM
     * won't manipulate MSRs directly.
     */

    void r_before_rw() { asm volatile("dmb ishld" : : : "memory"); }

    void w_before_w() { asm volatile("dmb ishst" : : : "memory"); }

    void rw_before_rw() { asm volatile("dmb ish" : : : "memory"); }

}
