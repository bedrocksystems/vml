/**
 * Copyright (c) 2019-2022 BedRock Systems, Inc.
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/utility.hpp>

/**
 * \brief for reading/writing memory that's not owned by the C++ abstract machine
 *
 * You don't have `primR` ownership of such memory.
 * Instead, your translation unit (e.g., the vswitch application)
 * treats such accesses as external MemRead/MemWrite events.
 *
 * You're given the right to perform these accesses by whoever gives you
 * committers for the shared memory. In the vSwitch case, the VMM gives
 * the committers. The committers may access internal invariants. For example,
 * in the vSwitch, the committer would open the invariant that holds the
 * [pbyte_at] facts for the guest memory.
 */

class ForeignPtr;

class ForeignData {
private:
    volatile void *_p{nullptr};

    // Make [ForeignPtr] a [friend] and keep the constructors private to ensure that
    // clients always access [ForeignData] instances via a [ForeignPtr].

    friend ForeignPtr;

    explicit ForeignData(volatile void *p) : _p(p) {}
    explicit ForeignData(void *p) : ForeignData(static_cast<volatile void *>(p)) {}

public:
    ForeignData() {}
    ForeignData(const ForeignData &) = delete;
    ForeignData(ForeignData &&other) { cxx::swap(_p, other._p); };
    ForeignData &operator=(const ForeignData &) = delete;
    ForeignData &operator=(ForeignData &&other) {
        cxx::swap(_p, other._p);
        return *this;
    }

    template<typename T>
    ForeignData &operator=(const T &t) {
        *(static_cast<volatile T *>(_p)) = t;
        return *this;
    }

    template<typename T>
    explicit operator T() {
        return *(static_cast<volatile T *>(_p));
    }
};

template ForeignData &ForeignData::operator=(const uint8 &);
template ForeignData &ForeignData::operator=(const uint16 &);
template ForeignData &ForeignData::operator=(const uint32 &);
template ForeignData &ForeignData::operator=(const uint64 &);

template ForeignData::operator uint8();
template ForeignData::operator uint16();
template ForeignData::operator uint32();
template ForeignData::operator uint64();

class ForeignPtr {
private:
    volatile void *_p{nullptr};

public:
    ForeignPtr() {}
    explicit ForeignPtr(volatile void *p) : _p(p) {}
    explicit ForeignPtr(void *p) : ForeignPtr(static_cast<volatile void *>(p)) {}

    ForeignPtr(const ForeignPtr &) = delete;
    ForeignPtr &operator=(const ForeignPtr &) = delete;

    ForeignPtr(ForeignPtr &&other) { cxx::swap(_p, other._p); }
    ForeignPtr &operator=(ForeignPtr &&other) {
        cxx::swap(_p, other._p);
        return *this;
    }

    ForeignPtr operator+(size_t index) const { return ForeignPtr(static_cast<volatile char *>(_p) + index); }

    ForeignData operator*() const { return ForeignData(_p); }

    ForeignData operator[](size_t index) const {
        // Other ways of writing this (note that [this] is a regular pointer,
        // and overloaded operators are only used on instances directly):
        // 1) return *(this->operator+(index));
        // 2) return (*this + index).operator*();
        // 3) return this->operator+(index).operator*();
        return *(*this + index);
    }
};
