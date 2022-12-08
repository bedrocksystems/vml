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

#include <platform/bits.hpp>
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

static inline mword
align_mmap(mword &offset, mword &size) {
    size_t pagesize = static_cast<size_t>(getpagesize());
    mword aligned_off = align_dn(offset, pagesize);
    mword offset_in_page = offset - aligned_off;
    size_t aligned_size = align_up(size + offset_in_page, pagesize);

    offset = aligned_off;
    size = aligned_size;

    return offset_in_page;
}

static inline void *
Platform::Mem::map_mem(const Platform::Mem::MemDescr &descr, mword offset, size_t size, int flags,
                       MemSel) {
    mword offset_in_page = align_mmap(offset, size);
    void *res = mmap(nullptr, size, flags, MAP_SHARED, static_cast<int>(descr.msel()),
                     static_cast<long>(offset));
    if (res == MAP_FAILED) {
        perror("mmap");
        return nullptr;
    }

    return reinterpret_cast<void *>(reinterpret_cast<mword>(res) + offset_in_page);
}

static inline bool
Platform::Mem::unmap_mem(const void *addr, size_t size) {
    mword offset = reinterpret_cast<mword>(addr);
    align_mmap(offset, size);
    int r = munmap(const_cast<void *>(addr), size);
    return r == 0;
}
