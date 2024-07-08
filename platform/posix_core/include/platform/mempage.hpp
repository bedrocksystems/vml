/*
 * Copyright (C) 2022 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

inline long
__get_page_size() {
    return sysconf(_SC_PAGESIZE);
}

#define PAGE_SIZE __get_page_size()
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_BITS static_cast<mword>(log2(PAGE_SIZE))
