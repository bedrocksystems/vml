/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
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
     */

    void r_before_rw() { asm volatile("lfence" : : : "memory"); }

    void w_before_w() { asm volatile("sfence" : : : "memory"); }

    void rw_before_rw() { asm volatile("mfence" : : : "memory"); }

}
