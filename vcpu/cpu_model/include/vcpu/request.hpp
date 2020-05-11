/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <platform/atomic.hpp>
#include <platform/types.hpp>

namespace Request {
    enum Requestor : uint32 {
        REQUESTOR_VMM = (1 << 0),
        REQUESTOR_VMI = (1 << 1),
    };

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
