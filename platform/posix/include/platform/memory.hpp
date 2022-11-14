/*
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <math.h>
#include <sys/mman.h>
#include <unistd.h>

#include <platform/mempage.hpp>
#include <platform/types.hpp>

namespace Platform::Mem {

    static constexpr int READ = PROT_READ;
    static constexpr int WRITE = PROT_WRITE;
    static constexpr int EXEC = PROT_EXEC;

    typedef uint64 MemSel;
    static constexpr MemSel REF_MEM{~0UL};

    class MemDescr;
    class Cred;

    static inline void *map_mem(const MemDescr &descr, mword offset, size_t size, int flags,
                                MemSel);
    static inline bool unmap_mem(const void *addr, size_t length);
};

class Platform::Mem::Cred {
public:
    bool write() const { return true; }
    bool read() const { return true; }
    bool uexec() const { return true; }
    bool sexec() const { return true; }
};

class Platform::Mem::MemDescr {
public:
    MemDescr(MemSel fd) : _memrange_sel(fd) {}

    MemDescr() : _memrange_sel(~0UL) {}

    MemSel msel() const { return _memrange_sel; }
    Platform::Mem::Cred cred() const { return _cred; }

private:
    MemSel _memrange_sel{~0UL};
    Platform::Mem::Cred _cred;
};

static inline void *
Platform::Mem::map_mem(const Platform::Mem::MemDescr &descr, mword offset, size_t size, int flags,
                       MemSel) {
    return mmap(nullptr, size, flags, MAP_SHARED, static_cast<int>(descr.msel()),
                static_cast<long>(offset));
}

static inline bool
Platform::Mem::unmap_mem(const void *addr, size_t length) {
    int r = munmap(const_cast<void *>(addr), length);
    return r == 0;
}
