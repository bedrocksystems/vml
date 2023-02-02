/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

/*! \file Simple Debugging facility for the VML code
 */
namespace Debug {
    static constexpr bool GUEST_MAP_ON_DEMAND = false;
    static constexpr bool TRACE_PAGE_PERMISSIONS = false;

    /*!
     * Reflect the level of debugging logic desired from the code.
     * Each library is responsible for its own usage of each level.
     */
    enum Level : unsigned int {
        NONE = 0,      /*!< No debugging enabled */
        CONDENSED = 1, /*!< Summarized debugging information/logic */
        DETAILLED = 2, /*!< Non-summarized debugging information/logic */
        FULL = 3,      /*!< All debugging facilities enabled - Very intrusive! */
    };

    /*! \brief Current debugging level. The final binary is responsible for defining this variable.
     */
    extern enum Level current_level;

    inline bool enabled() {
        return Debug::current_level > Debug::Level::NONE;
    }
};

namespace Stats {
    /*! \brief Indicates whether stats should be collected.
     */
    extern bool requested;

    inline bool enabled() {
        return (Debug::enabled() or requested);
    }
}
