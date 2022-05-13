/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>

template<typename T>
static inline constexpr T
min(T v1, T v2) {
    return v1 < v2 ? v1 : v2;
}

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

// NOTE: [align] must be a power of two
inline constexpr uint64_t
align_dn(uint64_t addr, uint64_t align) {
    addr &= ~(align - 1);
    return addr;
}

// NOTE: [align] must be a power of two
inline constexpr uint64_t
align_up(uint64_t addr, uint64_t align) {
    addr += (align - 1);
    return align_dn(addr, align);
}

inline constexpr uint64
combine_low_high(uint32 low, uint32 high) {
    return static_cast<uint64>(low) | (static_cast<uint64>(high) << 32);
}
