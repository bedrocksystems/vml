/*
 * Copyright (c) 2022-2024 BlueRock Security, Inc.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <vector>

/**
 *  Vector class.
 */

template<typename T>
class Vector : public std::vector<T> {

public:
    bool insert(T& item) {
        std::vector<T>::push_back(item);
        return true;
    }
    void reset() { std::vector<T>::clear(); }

    template<typename CLOSURE>
    void forall(CLOSURE run, bool reverse = false) const {
        for (size_t i = 0; i < std::vector<T>::size(); i++) {
            size_t index = reverse ? std::vector<T>::size() - i - 1 : i;
            run(index, std::vector<T>::at(index));
        }
    }

    bool is_empty() const { return std::vector<T>::empty(); }
};
