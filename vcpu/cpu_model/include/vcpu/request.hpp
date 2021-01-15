/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/atomic.hpp>
#include <platform/types.hpp>

namespace Request {
    enum Requestor : uint32 {
        VMM = (1 << 0),
        VMI = (1 << 1),
    };

    inline bool is_requested_by(Requestor requestor, const atomic<uint32> &requests) {
        uint32 expected = requests.load();

        return expected & requestor;
    }

    inline bool needs_update(Requestor requestor, bool enable, atomic<uint32> &requests) {
        uint32 previous = requests.load();

        if (enable) {
            previous = requests.fetch_or(requestor);
            if (previous == 0) {
                return true;
            }
        } else {
            previous = requests.fetch_and(~requestor);
            uint32 desired = previous & ~requestor;
            if (desired == 0) {
                if (previous == 0) {
                    return false;
                }
                return true;
            }
        }
        return false;
    }

} // namespace Request
