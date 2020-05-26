/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <errno.hpp>
#include <guest_config/guest.hpp>
#include <log/log.hpp>
#include <types.hpp>

Errno
check_image_header(mword const header_addr, mword const guest_map_addr, uint64 &image_size) {
    static constexpr uint32 kernel_magic = 0x644d5241 /* "ARM\x64" */;
    Kernel_header *kernel = reinterpret_cast<Kernel_header *>(header_addr);

    if (kernel->magic == kernel_magic) {
        if (((guest_map_addr & kernel->text_offset) != kernel->text_offset)
            || ((guest_map_addr & (kernel->text_offset - 1)))
            || ((guest_map_addr & ((1UL << 21) - 1)) != kernel->text_offset)) {
            WARN("kernel image is misaligned");
            return Errno::EINVAL;
        }

        image_size = kernel->image_size;
    } else
        INFO("Unknown kernel image type");

    return Errno::ENONE;
}
