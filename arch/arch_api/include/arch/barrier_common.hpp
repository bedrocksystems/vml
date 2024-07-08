/**
 * Copyright (C) 2023 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

/* Arch-independent interfaces for arch-dependent memory barriers. */
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

    static inline void r_before_rw(void);
    static inline void w_before_w(void);
    static inline void rw_before_rw(void);
    static inline void system(void);
    static inline void instruction(void);
}
