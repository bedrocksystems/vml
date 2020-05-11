/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/mem_util.hpp>
#include <platform/types.hpp>

void
flush_data_cache(void* start, size_t size) {
    uint64 cache_line_size = 64; // Could we query this at run-time?

    for (size_t i = 0; i < size; i += cache_line_size) {
        const char* addr = reinterpret_cast<char*>(start) + i;

        asm volatile("clflush (%0)" : : "r"(addr) : "memory");
    }
}