/**
 * Copyright (C) 2021-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#pragma once

#include <platform/types.hpp>

template<typename T>
T
mod(T a, T m) {
    return ((a % m) + m) % m;
}

template<typename T = mword, uint32 FIFO_MAX_SIZE = 0>
class SeqQueue {
public:
    bool is_empty() const { return _ridx == _current_capacity; }

    bool is_full() const { return _widx == _ridx; }

    uint32 cur_size() const {
        if (is_empty())
            return 0;
        else if (is_full())
            return _current_capacity;
        else
            return ((_widx + _current_capacity) - _ridx) % _current_capacity;
    }

    bool enqueue(T c) {
        if (is_full())
            return false;

        _data[_widx++] = c;

        _widx %= _current_capacity;

        // the if body is is_empty() but using it makes FM complex because we are not in a
        // consistent state yet. can use it if we write a separate spec of is_empty() that allows an
        // inconstent state at the beginning
        if (_ridx == _current_capacity)
            _ridx = 0;
        return true;
    }

    T dequeue() {
        T value = _data[_ridx++];
        _ridx %= _current_capacity;
        if (_ridx == _widx) { // empty
            _widx = 0;
            _ridx = _current_capacity;
        }

        return value;
    }

    uint32 get_current_capacity() { return _current_capacity; }

    T test() { return _data[0]; }

    void reset(uint32 new_cap) {
        _current_capacity = new_cap;
        _widx = 0;
        _ridx = _current_capacity;

        // if this library is called by unverified code, uncomment the following. specs prevent leak
        // in verified code
        // for (uint32 i = 0; i < FIFO_MAX_SIZE; i++)
        //     _data[i] = 0; // Reset error status to zero
    }

    void reset_maximize_capacity() { reset(FIFO_MAX_SIZE); }

private:
    uint32 _widx{0};                 /*!< Write index in the FIFO */
    T _data[FIFO_MAX_SIZE] = {};     /*!< Receive FIFO */
    uint32 _current_capacity{1};     /*!< Maximum configured size */
    uint32 _ridx{_current_capacity}; /*!< Read index in the FIFO */
};

template class SeqQueue<uint16, 32>;
