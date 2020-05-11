/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <errno.hpp>
#include <types.hpp>

constexpr uint32 LINUX_MACHINE_TYPE_DTB = ~0x0u;

/* linux kernel header parsing - linux/Documentation/arm64/booting.txt */
struct Kernel_header {
    uint64 code;
    uint64 text_offset;
    uint64 image_size;
    uint64 flags;
    uint64 res2;
    uint64 res3;
    uint64 res4;
    uint32 magic;
    uint32 res5;
};

Errno check_image_header(mword const header_addr, mword const guest_map_addr, uint64 &image_size);
