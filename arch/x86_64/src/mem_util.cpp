/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <arch/mem_util.hpp>
#include <platform/types.hpp>

void
icache_sync_range(void* start, size_t size) {
    uint64 cache_line_size = 64; // Could we query this at run-time?

    for (size_t i = 0; i < size; i += cache_line_size) {
        const char* addr = reinterpret_cast<char*>(start) + i;

        asm volatile("clflush (%0)" : : "r"(addr) : "memory");
    }
}
