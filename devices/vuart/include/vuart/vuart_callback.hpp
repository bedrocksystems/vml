/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>

struct VcpuCtx;

namespace Vuart {
    class Tx_callback;
    class LifeCycleCallbacks;
}

/*! \brief Callback interface to send chars to the backend
 */
class Vuart::Tx_callback {
public:
    /*! \brief Will be called when the vuart needs to send chars to the outside
     *  \param c character to send
     *  \return number of characters written - 1 for now
     *  \pre Valid callback object, valid UMX connection
     *  \post Valid callback object, c was sent to the outside world (best effort basis)
     */
    virtual uint32 from_guest_sent(char c) = 0;
};

/*! \brief Callback interface for reset/shutdown events
 */
class Vuart::LifeCycleCallbacks {
public:
    virtual void device_reset(const VcpuCtx *) = 0;
    virtual void shutdown(const VcpuCtx *) = 0;
};