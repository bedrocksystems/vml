/*
 * Copyright (C) 2020 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <arch/breakpoint.hpp>
#include <platform/compiler.hpp>

unsigned
Breakpoint::get_size(Type t) {
    switch (t) {
    case Breakpoint::Type::DEFAULT:
    case Breakpoint::Type::BRK_A32_BIT:
    case Breakpoint::Type::BRK_A64_BIT:
        return 4;
    case Breakpoint::Type::BRK_A16_BIT:
        return 2;
    default:
        __UNREACHED__;
    }
}

uint64
Breakpoint::get_instruction(Type t, uint16 id) {
    static constexpr uint64 BRK_AA64 = 0xd4200540ull;
    static constexpr uint64 BKPT_AA32 = 0xe120027aull;
    static constexpr uint64 BKPT_T = 0xbe2aull;
    uint64 instruction;
    uint64 lid = id;

    switch (t) {
    case Breakpoint::Type::DEFAULT:
    case Breakpoint::Type::BRK_A64_BIT:
        instruction = (BRK_AA64 & 0xffe0001full) | ((lid << 5) & 0x1fffe0ull);
        break;
    case Breakpoint::Type::BRK_A32_BIT:
        instruction = (BKPT_AA32 & 0xfff000f0ull) | ((lid << 4) & 0xfff00ull) | (id & 0xfull);
        break;
    case Breakpoint::Type::BRK_A16_BIT:
        instruction = (BKPT_T & 0xff00ull) | (id & 0xffull);
        break;
    default:
        __UNREACHED__;
    }

    return instruction;
}
