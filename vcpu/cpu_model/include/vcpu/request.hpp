/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

#include <platform/atomic.hpp>
#include <platform/types.hpp>

namespace Request {
    enum Requestor : uint32 {
        REQUESTOR_VMM = (1 << 0),
        REQUESTOR_VMI = (1 << 1),
    };

    inline bool is_requested_by(Requestor requestor, const atomic<uint32> &requests) {
        uint32 expected = requests.load();

        return expected & requestor;
    }

    inline bool needs_update(Requestor requestor, bool enable, atomic<uint32> &requests) {
        uint32 expected = requests.load();
        uint32 desired;

        if (enable) {
            do {
                desired = expected | requestor;
            } while (!requests.cas(expected, desired));
            if (expected == 0) {
                return true;
            }
        } else {
            do {
                desired = expected & ~requestor;
            } while (!requests.cas(expected, desired));
            if (desired == 0) {
                if (expected == 0) {
                    return false;
                }
                return true;
            }
        }
        return false;
    }

} // namespace Request
