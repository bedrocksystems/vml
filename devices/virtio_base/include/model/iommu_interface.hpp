/**
 * Copyright (C) 2023 BedRock Systems, Inc.
 * All rights reserved.
 */

#pragma once

#include <platform/errno.hpp>
#include <platform/new.hpp>
#include <platform/rangemap.hpp>

// This file implements an interface to plug VirtIO device models with a virtual IOMMU implementation

namespace Model {
    enum class IOMappingFlags : uint32;
    struct IOMapping;
    struct IOMappingNode;
    using IOMappingTable = RangeMap<uint64>;
    class IOMMUManagedDevice;
}

enum class Model::IOMappingFlags : uint32 {
    NONE = 0,
    READ = 1ull << 0,
    WRITE = 1ull << 1,
};

/*
 * An IO Mapping. Serializable
 *
 */
struct Model::IOMapping {
    IOMapping() : va(UINT64_MAX), pa(UINT64_MAX), sz(0), flags(Model::IOMappingFlags::NONE) {}
    IOMapping(uint64 virt_addr, uint64 phys_addr, uint64 range_size, IOMappingFlags mapping_flags)
        : va(virt_addr), pa(phys_addr), sz(range_size), flags(mapping_flags) {}

    bool write() const { return 0 != (static_cast<uint32>(flags) & static_cast<uint32>(IOMappingFlags::WRITE)); }
    bool read() const { return 0 != (static_cast<uint32>(flags) & static_cast<uint32>(IOMappingFlags::READ)); }

    // NOTE: An IO mapping will always be contiguous
    uint64 va; // Virtual start
    uint64 pa; // Physical start
    uint64 sz; // Interval size
    IOMappingFlags flags;
};
static_assert(sizeof(Model::IOMapping) == 32);

/*
 * IO Mapping node for tracking within an IO mapping table
 *
 */
struct Model::IOMappingNode : RangeNode<uint64>, public Model::IOMapping {
    IOMappingNode() = delete;
    IOMappingNode(uint64 phys_addr, IOMappingFlags mapping_flags, const Range<uint64> &r)
        : RangeNode(r), IOMapping(r.begin(), phys_addr, r.size(), mapping_flags) {}
    explicit IOMappingNode(const Model::IOMapping &&m) : RangeNode(Range<uint64>(m.va, m.sz)), IOMapping(m) {}
};

/*
 * Callback interface for virtual endpoints managed by a virtual IOMMU
 * It implements a basic IO translation mechanism for VirtIO devices. Devices that require more elaborate mechanisms can override
 * the virtual methods.
 * This interface does not handle concurrency. Client should override to handle concurrency as per their requirement
 *
 */
class Model::IOMMUManagedDevice {
public:
    // Lets the client know that a virtual IOMMU is present within the system and initialized by the guest
    // NOTE: For the purpose of address translations, an attach event is sufficient. [iommu_present] enables certain policy
    // decisions e.g. if a guest decides not to use IO protection for a device even in the presence of a virtual IOMMU
    virtual void iommu_present() { iommu_avail = true; }

    // Endpoint corresponding to [this] device attached to an IOMMU domain
    virtual void attach() { attached = true; }

    // Endpoint corresponding to [this] device detached from an IOMMU domain.
    // A detach also invalidates any IO mappings for this device [detach == unmap all]
    virtual void detach() {
        remove_all_mappings();
        attached = false;
    }

    // MAP request
    virtual Errno map(const Model::IOMapping &m) {
        Range<uint64> r{m.va, m.sz};
        Model::IOMappingNode *n = new (nothrow) Model::IOMappingNode(m.pa, m.flags, r);
        if (nullptr == n)
            return Errno::NOMEM;

        io_mappings.insert(n);

        return Errno::NONE;
    }

    // UNMAP request
    virtual Errno unmap(const Model::IOMapping &m) {
        Range<uint64> r{m.va, m.sz};
        Model::IOMappingNode *n = static_cast<Model::IOMappingNode *>(io_mappings.remove(r));
        delete n;
        return Errno::NONE;
    }

    // Translate an IO address based on the mappings available here.
    virtual uint64 translate_io(uint64 io_addr, size_t size_bytes) const {
        Range<uint64> r{io_addr, size_bytes};
        Model::IOMappingNode *n = static_cast<Model::IOMappingNode *>(io_mappings.lookup(&r));
        if (nullptr == n) {
            return ~0ull;
        }

        // An IO mapping maps a virtually contiguous range (va, sz) to a physically contiguous range (pa, sz)
        // An IO address is an offset within the VA range and the corresponding physical address is the same offset within the
        // physical range: [Physical Address] = IO/Virtual Address - [Virtual Start] + [Physical Start]
        return io_addr - n->va + n->pa;
    }

    static void remove_mapping(RangeNode<uint64> *n) { delete static_cast<Model::IOMappingNode *>(n); }
    void remove_all_mappings() { io_mappings.clear(Model::IOMMUManagedDevice::remove_mapping); }

    void reset() {
        remove_all_mappings();
        iommu_avail = false;
        attached = false;
    }

public:
    bool iommu_avail{false};
    bool attached{false};
    Model::IOMappingTable io_mappings;
};
