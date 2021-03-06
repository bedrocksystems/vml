/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>

inline constexpr uint64_t
mask(uint8_t order, unsigned offset = 0) {
    return (order >= (sizeof(uint64_t) * 8) ? ~0ull : ((1ull << order) - 1)) << offset;
}

inline constexpr uint64_t
bits(uint64_t val, uint8_t count, uint8_t from) {
    return (val >> from) & mask(count);
}

inline constexpr uint64_t
bits_in_range(uint64_t val, uint8_t start, uint8_t end) {
    return bits(val, static_cast<uint8>(end - start + 1), start);
}
