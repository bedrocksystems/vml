/**
 * Copyright (c) 2019-2024 BlueRock Security, Inc.
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <platform/type_traits.hpp>
#include <utility>

namespace cxx {
    template<class T>
    inline void swap(T& x, T& y) {
        typename cxx::remove_reference<T>::type t = move(x);
        x = move(y);
        y = move(t);
    }

    template<typename T1, typename T2>
    using Pair = std::pair<T1, T2>;
};
