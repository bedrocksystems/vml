/*
 * Copyright (C) 2023 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#pragma once

#include <platform/errno.hpp>

namespace PS2 {
    class PS2Dev;
}

class PS2::PS2Dev {
public:
    Errno read_data(uint8 &data);
    Errno write_data(uint8 data);

    Errno read_status(uint8 &status);
    Errno write_command(uint8 command);

    // Errno get_irq(uint8 idx, BHV::Sel &irq_sel, BHV::IrqSem::Perms );
};
