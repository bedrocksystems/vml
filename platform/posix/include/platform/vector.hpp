/*
 * Copyright (c) 2022 BedRock Systems, Inc.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/**
 *  Dummy stub for Vector class.
 */

template<typename T>
class Vector {

public:
    Vector() = delete;
    bool insert(T) { return false; }
    void reset() {}
};
