/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>

namespace Vuart {
    class Tx_callback;
}

/*! \brief Callback interface to send chars to the backend
 */
class Vuart::Tx_callback {
public:
    /*! \brief Will be called when the vuart needs to send chars to the outside
     *  \param c character to send
     *  \return number of characters written - 1 for now
     */
    virtual uint32 from_guest_sent(const char &c) = 0;
};