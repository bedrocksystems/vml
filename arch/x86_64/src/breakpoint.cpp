/*
 * Copyright (C) 2022 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <arch/breakpoint.hpp>
#include <platform/compiler.hpp>

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
