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
    enum Level {
        None = 0,      /*!< No debugging enabled */
        Condensed = 1, /*!< Summarized debugging information/logic */
        Detailled = 2, /*!< Non-summarized debugging information/logic */
        Full = 3,      /*!< All debugging facilities enabled - Very intrusive! */
    };

    /*! \brief Current debugging level. The final binary is responsible for defining this variable.
     */
    extern enum Level current_level;
};
