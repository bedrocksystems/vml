/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <platform/types.hpp>

namespace Model {
    struct Page_info;
};

/*! \brief Represent the permissions currently set on a page
 */
struct Page_permission {
    /*! \brief Constructor: a page is set to RWX by default
     */
    Page_permission(bool read = true, bool write = true, bool exec = true)
        : r(read), w(write), x(exec) {}

    bool operator==(const Page_permission& other) const {
        return other.r == r && other.w == w && other.x == x;
    }

    bool operator!=(const Page_permission& other) const { return !(*this == other); }

    uint8 r : 1;
    uint8 w : 1;
    uint8 x : 1;
};