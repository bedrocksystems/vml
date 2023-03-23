/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>

namespace Msr {
    class Id;

    static constexpr uint32 build_msr_id(uint8 const op0, uint8 const crn, uint8 const op1, uint8 const crm, uint8 const op2) {
        return (((static_cast<uint32>(crm) & 0xfu) << 3) | ((static_cast<uint32>(crn) & 0xfu) << 7)
                | ((static_cast<uint32>(op1) & 0x7u) << 10) | ((static_cast<uint32>(op2) & 0x7u) << 13)
                | ((static_cast<uint32>(op0) & 0xffu) << 16));
    }
}

class Msr::Id {
private:
    uint32 _id;

public:
    /* align id 8 byte for vbus usage */
    Id(uint8 const op0, uint8 const crn, uint8 const op1, uint8 const crm, uint8 const op2)
        : _id(build_msr_id(op0, crn, op1, crm, op2)) {}

    /*
     * We are bending the rules a bit for this class. It is useful to have a conversion from uint32
     * to an ID without being explicit because most IDs are stored in an enum as uint32. We still
     * want to use those uint32 as Ids transparently whenever possible.
     */
    Id(uint32 id) : _id(id) {} // NOLINT

    uint32 id() const { return _id; }
};
