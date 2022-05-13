/**
 * Copyright (c) 2019-2022 BedRock Systems, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/type_traits.hpp>

namespace cxx {
    template<class T>
    inline void swap(T& x, T& y) {
        typename cxx::remove_reference<T>::type t = move(x);
        x = move(y);
        y = move(t);
    }
};
