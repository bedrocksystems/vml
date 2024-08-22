/*
 * Copyright (C) 2022-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <arch/breakpoint.hpp>

static constexpr uint64 HLT_OPCODE = 0xF4;
static constexpr unsigned HLT_OPCODE_LEN = 1;

unsigned
Breakpoint::get_size(Type) {
    return HLT_OPCODE_LEN;
}

uint64
Breakpoint::get_instruction(Type, uint16) {
    return HLT_OPCODE;
}
