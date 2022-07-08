/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file
 *  \brief Wrapper around atomic, we expect atomic<T> to be defined
 */

#include <atomic>

/*! \brief Thin wrapper around std::atomic
 *
 *   This class should provide a CAS function and an assignment operator.
 */
template<typename T>
class atomic : public std::atomic<T> {
public:
    using std::atomic<T>::atomic;

    /*! \brief Compare Exchange
     *  \param e Expected value
     *  \param d Desired value
     *  \param weak Should the operation be weak or strong?
     *  \return true if the operation was successful, false otherwise.
     */
    inline bool cas(T& e, T d, bool weak = false) {
        if (weak)
            return std::atomic_compare_exchange_weak(this, &e, d);
        else
            return std::atomic_compare_exchange_strong(this, &e, d);
    }

    inline T add_fetch(T v) { return this->operator+=(v); }

    inline T sub_fetch(T v) { return this->operator-=(v); }

    inline T and_fetch(T v) { return this->operator&=(v); }

    inline T or_fetch(T v) { return this->operator|=(v); }

    /*! \brief Atomically assign the given value
     *  \param v value to assign
     *  \return the assigned value
     */
    inline T operator=(T v) {
        std::atomic_store(this, v);
        return v;
    }
};
