/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

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

    static inline void r_before_rw(void) { asm volatile("lfence" : : : "memory"); }

    static inline void w_before_w(void) { asm volatile("sfence" : : : "memory"); }

    static inline void rw_before_rw(void) { asm volatile("mfence" : : : "memory"); }

    static inline void instruction(void) {}

}
