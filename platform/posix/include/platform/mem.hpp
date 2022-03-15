/*
 * Copyright (c) 2022 BedRock Systems, Inc.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <sys/mman.h>

namespace Platform::Mem {

    static constexpr int READ = PROT_READ;
    static constexpr int WRITE = PROT_WRITE;
    static constexpr int EXEC = PROT_EXEC;

    void *map_mem(mword descr, mword offset, size_t size, int flags);
    bool unmap_mem(const void *addr, size_t length);
};

void *
Platform::Mem::map_mem(mword descr, mword offset, size_t size, int flags) {
    return mmap(nullptr, size, flags, MAP_SHARED, static_cast<int>(descr),
                static_cast<long>(offset));
}

bool
Platform::Mem::unmap_mem(const void *addr, size_t length) {
    int r = munmap(const_cast<void *>(addr), length);
    if (r != 0)
        return false;
    return true;
}
