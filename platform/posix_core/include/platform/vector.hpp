/*
 * Copyright (c) 2022 BedRock Systems, Inc.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
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
};
