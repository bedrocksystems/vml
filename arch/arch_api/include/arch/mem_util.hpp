/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <platform/types.hpp>

/*
 * Make sure that any data from start to start + size is flushed
 * out of the data cache and is committed to main memory.
 */
void flush_data_cache(void* start, size_t size);