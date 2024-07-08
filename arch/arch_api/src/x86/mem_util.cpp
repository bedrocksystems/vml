/**
 * Copyright (C) 2020 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <arch/mem_util.hpp>
#include <platform/types.hpp>

void
dcache_clean_range(void* start, size_t size) {
    uint64 cache_line_size = 64; // Could we query this at run-time?

    for (size_t i = 0; i < size; i += cache_line_size) {
        const char* addr = reinterpret_cast<char*>(start) + i;

        asm volatile("clflush (%0)" : : "r"(addr) : "memory");
    }
}

void
dcache_clean_invalidate_range(void* va_start, size_t size) {
    return dcache_clean_range(va_start, size);
}

void
icache_invalidate_range(void*, size_t) {
    return;
}

void
icache_sync_range(void* start, size_t size) {
    dcache_clean_range(start, size);
}
