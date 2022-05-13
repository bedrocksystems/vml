/**
 * Copyright (c) 2019-2022 BedRock Systems, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>

namespace cxx {
    template<class T>
    struct remove_reference {
        using type = T;
    };
    template<class T>
    struct remove_reference<T&> {
        using type = T;
    };
    template<class T>
    struct remove_reference<T&&> {
        using type = T;
    };

    template<class T>
    using remove_reference_t = typename remove_reference<T>::type;

    template<class T>
    inline constexpr typename cxx::remove_reference<T>::type&& move(T&& t) noexcept {
        return static_cast<typename cxx::remove_reference<T>::type&&>(t);
    }
};
