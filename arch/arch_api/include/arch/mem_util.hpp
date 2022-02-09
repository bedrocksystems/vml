/**
 * Copyright (C) 2019-2022 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>

void dcache_clean_range(void* va_start, size_t size);

void dcache_clean_invalidate_range(void* va_start, size_t size);

void icache_invalidate_range(void* va_start, size_t size);

void icache_sync_range(void* va_start, size_t size);
