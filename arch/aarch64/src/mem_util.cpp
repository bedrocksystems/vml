/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arch/barrier.hpp>
#include <arch/mem_util.hpp>
#include <msr/msr_info.hpp>
#include <platform/types.hpp>

void
flush_data_cache(void* start, size_t size) {
    Msr::Info::Ctr ctr;
    uint64 cache_line_size = ctr.cache_line_size();

    Barrier::rw_before_rw();
    for (size_t i = 0; i < size; i += cache_line_size) {
        const char* addr = reinterpret_cast<char*>(start) + i;

        asm volatile("dc civac, %0" : : "r"(addr) : "memory");
    }
    Barrier::rw_before_rw();
}