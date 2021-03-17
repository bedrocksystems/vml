/*
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <math.h>
#include <platform/types.hpp>
#include <unistd.h>

inline long
__get_page_size() {
    return sysconf(_SC_PAGESIZE);
}

#define PAGE_SIZE __get_page_size()
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_BITS static_cast<mword>(log2(PAGE_SIZE))
