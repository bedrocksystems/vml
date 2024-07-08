/**
 * Copyright (C) 2019-2022 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>

void dcache_clean_range(void* va_start, size_t size);

void dcache_clean_invalidate_range(void* va_start, size_t size);

void icache_invalidate_range(void* va_start, size_t size);

void icache_sync_range(void* va_start, size_t size);
