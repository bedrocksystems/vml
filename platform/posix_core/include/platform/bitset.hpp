/*
 * Copyright (C) 2020 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

/*! \file
 *  \brief Exposes an atomic bitfield
 */

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <platform/atomic.hpp>

/*! \brief Bitfield with atomic operations
 *
 *  We rely on std::atomic to keep the implementation
 *  simple. It is possible to make this code faster in the future.
 */
template<size_t SIZE>
class AtomicBitset {
public:
    AtomicBitset() { reset(); }

    /*! \brief Value returned by 'first_set' when no bit was set
     */
    static constexpr size_t NOT_FOUND = ~0x0ull;

    /*! \brief Search for the first bit set
     *  \param start first bit that will be considered
     *  \param len number of bits to consider starting from start
     *  \return the index of the first bit set or NOT_FOUND
     */
    size_t first_set(size_t start, size_t len) const {
        for (size_t i = start; i < std::min(start + len, SIZE); ++i)
            if (is_set(i))
                return i;

        return NOT_FOUND;
    }

    /*! \brief Set all bits to unset
     */
    void reset() {
        for (auto& f : _flags)
            std::atomic_store(&f, false);
    }

    /*! \brief Check if a bit is set
     *  \param bit index of the bit to check
     *  \return true if the bit is set, false otherwise
     */
    bool is_set(const size_t bit) const { return _flags[bit]; }

    /*! \brief Set the bit at index
     *  \param bit index of the bit to set
     */
    void set(const size_t bit) { std::atomic_store(&_flags[bit], true); }

    /*! \brief Clear the bit at index
     *  \param bit index of the bit to check
     */
    void clr(const size_t bit) { std::atomic_store(&_flags[bit], false); }

private:
    std::atomic<bool> _flags[SIZE];
};

template<size_t SIZE>
using Bitset = std::bitset<SIZE>;
