/*
 * Copyright (C) 2023 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#pragma once

#include <platform/errno.hpp>

// NOLINTBEGIN(readability-convert-member-functions-to-static)
namespace RTC {
    struct Date {
        uint8 seconds;
        uint8 minutes;
        uint8 hours;
        uint8 day_of_week;
        uint8 day;
        uint8 month;
        uint16 year;
    };

    class Rtc {
    public:
        Errno get_date(Date &) { return Errno::NODEV; }
        Errno set_date(Date &) { return Errno::NODEV; }
    };
};
// NOLINTEND(readability-convert-member-functions-to-static)
