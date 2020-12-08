/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <arch/barrier.hpp>
#include <arch/mem_util.hpp>
#include <msr/msr_info.hpp>
#include <platform/types.hpp>

static inline void
dcache_clean_line(mword va) {
    /* Clean (not invalidate) the data cache for the VA to PoU. */
    asm volatile("dc cvau, %0" : : "r"(va) : "memory");
}

static inline void
icache_invalidate_line(mword va) {
    /* Invalidate the cache line for the VA to PoU. */
    asm volatile("ic ivau, %0" : : "r"(va) : "memory");
}

void
dcache_clean_range(void* va_start, size_t size) {
    Msr::Info::Ctr ctr;
    uint64 cache_line_size = ctr.dcache_line_size();
    mword va = align_dn(reinterpret_cast<mword>(va_start), cache_line_size);
    mword va_end = align_up(reinterpret_cast<mword>(va_start) + size, cache_line_size);

    /* Make sure that previous writes have completed. */
    Barrier::rw_before_rw();

    /* Clean the data cache for the VA range to PoU. */
    for (; va < va_end; va += cache_line_size) {
        dcache_clean_line(va);
    }

    /* Make sure we finish all dcache clean operations. */
    Barrier::rw_before_rw();
}

void
icache_invalidate_range(void* va_start, size_t size) {
    Msr::Info::Ctr ctr;
    uint64 cache_line_size = ctr.icache_line_size();
    mword va = align_dn(reinterpret_cast<mword>(va_start), cache_line_size);
    mword va_end = align_up(reinterpret_cast<mword>(va_start) + size, cache_line_size);

    /* Invalidate the instruction cache for the VA range to PoU. */
    for (; va < va_end; va += cache_line_size) {
        icache_invalidate_line(va);
    }

    /* Make sure we finish the icache invalidation. */
    Barrier::rw_before_rw();
    Barrier::instruction();
}

void
icache_sync_range(void* va_start, size_t size) {
    dcache_clean_range(va_start, size);
    icache_invalidate_range(va_start, size);
}
