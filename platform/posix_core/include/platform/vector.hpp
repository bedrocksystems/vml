/*
 * Copyright (c) 2022 BlueRock Security, Inc.
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
};
