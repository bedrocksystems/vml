/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <platform/types.hpp>

/*! \file Architecture independent API to manipulate breakpoint instructions.
 */

namespace Breakpoint {

    /*! \brief Distinguish between all types of breakpoints supported.
     *
     * For certain architectures, all those breakpoints might point to the
     * same underlying instruction. For others, like ARM, it might map to
     * 3 different types of breakpoints with different length and encoding.
     */
    enum Type {
        DEFAULT,     /*!< Will map to A64 or A32 depending on the initial boot mode of the VM */
        BRK_A64_BIT, /*!< Force the use of 64-bit mode breakpoint */
        BRK_A32_BIT, /*!< Force the use of 32-bit mode breakpoint */
        BRK_A16_BIT, /*!< Force the use of 16-bit mode (or Thumb on ARM) breakpoint */
    };

    /*! \brief Returns the size of the breakpoint instruction with the given type
     *  \param t The type of the breakpoint
     *  \return The number of bytes that the instruction takes
     */
    unsigned get_size(Type t);

    /*! \brief Returns the byte code of the breakpoint instruction
     *  \param t The type of the breakpoint
     *  \param id ID to be included in the breakpoint instruction (may not be supported all
     *            Architectures)
     *  \return The byte codes - Encoded with the endianess of the platform
     */
    uint64 get_instruction(Type t, uint16 id = 0);
}
