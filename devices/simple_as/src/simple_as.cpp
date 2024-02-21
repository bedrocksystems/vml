/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <arch/barrier.hpp>
#include <arch/mem_util.hpp>
#include <model/simple_as.hpp>
#include <platform/compiler.hpp>

#include <platform/string.hpp>

// Alignment is ensured by the caller but the compiler does not know this
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
uint64
Model::SimpleAS::single_mapped_read(void* ptr, uint8 size) {
    ASSERT(size <= sizeof(uint64));
    ASSERT(size != 0); // Guaranteed by the Vbus
    ASSERT(ptr != nullptr);

    uint64 ret;

    if (__UNLIKELY__((reinterpret_cast<mword>(ptr) % size) != 0)) {
        // Unaligned data
        memcpy(&ret, ptr, size);
        return ret;
    }

    switch (size) {
    case sizeof(uint8):
        ret = *(reinterpret_cast<uint8*>(ptr));
        break;
    case sizeof(uint16):
        ret = *(reinterpret_cast<uint16*>(ptr));
        break;
    case sizeof(uint32):
        ret = *(reinterpret_cast<uint32*>(ptr));
        break;
    case sizeof(uint64):
        ret = *(reinterpret_cast<uint64*>(ptr));
        break;
    default:
        ABORT_WITH("Read size %u is not supported", size);
        __UNREACHED__;
    }

    return ret;
}

void
Model::SimpleAS::single_mapped_write(void* ptr, uint8 size, uint64 value) {
    ASSERT(size <= sizeof(uint64));
    ASSERT(size != 0); // Guaranteed by the Vbus
    ASSERT(ptr != nullptr);

    if (__UNLIKELY__((reinterpret_cast<mword>(ptr) % size) != 0)) {
        // Unaligned data
        memcpy(ptr, &value, size);
    } else {
        switch (size) {
        case sizeof(uint8):
            *(reinterpret_cast<uint8*>(ptr)) = static_cast<uint8>(value);
            break;
        case sizeof(uint16):
            *(reinterpret_cast<uint16*>(ptr)) = static_cast<uint16>(value);
            break;
        case sizeof(uint32):
            *(reinterpret_cast<uint32*>(ptr)) = static_cast<uint32>(value);
            break;
        case sizeof(uint64):
            *(reinterpret_cast<uint64*>(ptr)) = static_cast<uint64>(value);
            break;
        default:
            ABORT_WITH("Write size %u is not supported", size);
            __UNREACHED__;
        }
    }

    icache_sync_range(ptr, size);
}
#pragma GCC diagnostic pop

uint64
Model::SimpleAS::single_access_read(uint64 off, uint8 size) const {
    ASSERT(size <= sizeof(uint64));

    uint64 ret;
    void* ptr;
    if (!mapped()) {
        ptr = map_view(off, size, true);
    } else {
        ptr = get_vmm_view() + off;
    }

    if (ptr == nullptr)
        ABORT_WITH("could not map offset 0x%llx sz 0x%x", off, size);

    ret = single_mapped_read(ptr, size);

    if (!mapped()) {
        unmap_guest_mem(ptr, size);
    }
    return ret;
}

void
Model::SimpleAS::single_access_write(uint64 off, uint8 size, uint64 value) const {
    ASSERT(size <= sizeof(uint64));

    void* ptr;
    if (!mapped()) {
        ptr = map_view(off, size, true);
    } else {
        ptr = get_vmm_view() + off;
    }

    if (ptr == nullptr)
        ABORT_WITH("could not map offset 0x%llx sz 0x%x", off, size);

    single_mapped_write(ptr, size, value);

    if (!mapped()) {
        unmap_guest_mem(ptr, size);
    }
}

Errno
Model::SimpleAS::read(char* dst, size_t size, const GPA& addr) const {
    if (!is_gpa_valid(addr, size))
        return Errno::INVAL;

    mword offset = addr.get_value() - get_guest_view().get_value();
    void* src;

    if (!mapped()) {
        src = Platform::Mem::map_mem(_mobject, offset, size, Platform::Mem::READ, get_mem_fd().msel());
        if (src == nullptr)
            return Errno::NOMEM;
    } else {
        src = get_vmm_view() + offset;
    }

    memcpy(dst, src, size);

    /* unmap */
    if (!mapped()) {
        bool b = Platform::Mem::unmap_mem(src, size);
        if (!b)
            ABORT_WITH("Unable to unmap region");
    }
    return Errno::NONE;
}

Errno
Model::SimpleAS::write(const GPA& gpa, size_t size, const char* src) const {
    void* dst;

    Errno err = demand_map(gpa, size, dst, true);
    if (Errno::NONE != err) {
        return err;
    }

    memcpy(dst, src, size);

    if (_flush_on_write)
        return demand_unmap_clean(gpa, size, dst);

    return demand_unmap(gpa, size, dst);
}

Errno
Model::SimpleAS::demand_map(const GPA& gpa, size_t size_bytes, void*& va, bool write) const {
    if (!is_gpa_valid(gpa, size_bytes))
        return Errno::INVAL;

    if (write && is_read_only() && !get_mem_fd().cred().write()) {
        WARN("Cannot map read-only guest memory for write pa:0x%llx size:0x%lx", gpa.get_value(), size_bytes);
        return Errno::PERM;
    }

    mword offset = gpa.get_value() - get_guest_view().get_value();
    if (!mapped()) {
        DEBUG("demand_map pa:0x%llx size:0x%lx write:%d (+0x%lx)", gpa.get_value(), size_bytes, write, offset);
        va = Platform::Mem::map_mem(_mobject, offset, size_bytes, Platform::Mem::READ | (write ? Platform::Mem::WRITE : 0),
                                    get_mem_fd().msel());
        if (va == nullptr) {
            WARN("Unable to map a chunk pa:%llx size:0x%lx", gpa.get_value(), size_bytes);
            return Errno::NOMEM;
        }
    } else {
        va = get_vmm_view() + offset;
    }

    return Errno::NONE;
}

Errno
Model::SimpleAS::demand_unmap(const GPA&, size_t size_bytes, void* va) const {
    if (!mapped()) {
        DEBUG("demand_unmap mem:0x%p size:0x%lx", va, size_bytes);
        bool b = Platform::Mem::unmap_mem(va, size_bytes);
        if (!b)
            ABORT_WITH("Unable to unmap guest memory mem:0x%p size:0x%lx", va, size_bytes);
    }

    return Errno::NONE;
}

Errno
Model::SimpleAS::demand_unmap_clean(const GPA&, size_t size_bytes, void* va) const {
    dcache_clean_range(va, size_bytes);
    icache_invalidate_range(va, size_bytes);

    if (!mapped()) {
        DEBUG("demand_unmap_clean mem:0x%p size:0x%lx", va, size_bytes);

        bool b = Platform::Mem::unmap_mem(va, size_bytes);
        if (!b)
            ABORT_WITH("Unable to unmap guest memory mem:0x%p size:0x%lx", va, size_bytes);
    }

    return Errno::NONE;
}

void*
Model::SimpleAS::map_view(mword offset, size_t size, bool write) const {
    if (is_read_only() && write && !_mobject.cred().write())
        return nullptr;

    void* dst = Platform::Mem::map_mem(_mobject, offset, size, Platform::Mem::READ | (write ? Platform::Mem::WRITE : 0),
                                       get_mem_fd().msel());
    if (dst == nullptr)
        ABORT_WITH("Unable to map view of the guest region:0x%llx offset:0x%lx "
                   "size:0x%lx",
                   get_guest_view().get_value(), offset, size);

    return dst;
}

Errno
Model::SimpleAS::clean_invalidate(GPA gpa, size_t size) const {
    if (!is_gpa_valid(gpa, size))
        return Errno::INVAL;

    mword offset = gpa.get_value() - get_guest_view().get_value();
    void* dst;

    if (!mapped()) {
        dst = Platform::Mem::map_mem(_mobject, offset, size, Platform::Mem::READ | Platform::Mem::WRITE, get_mem_fd().msel());
        if (dst == nullptr)
            return Errno::NOMEM;
    } else {
        dst = get_vmm_view() + offset;
    }

    dcache_clean_invalidate_range(dst, size);

    if (!mapped()) {
        bool b = Platform::Mem::unmap_mem(dst, size);
        if (!b)
            ABORT_WITH("Unable to unmap region");
    }
    return Errno::NONE;
}

void
Model::SimpleAS::flush_guest_as() {
    if (is_read_only() or not _flush_on_reset or not _mobject.cred().write())
        return;

    void* mapped_area;
    if (!mapped()) {
        mapped_area
            = Platform::Mem::map_mem(_mobject, 0, get_size(), Platform::Mem::READ | Platform::Mem::WRITE, get_mem_fd().msel());
        if (mapped_area == nullptr)
            ABORT_WITH("Unable to map guest region %llx", get_guest_view().get_value());
    } else {
        mapped_area = _vmm_view;
    }

    dcache_clean_invalidate_range(mapped_area, get_size());
    Barrier::system();

    if (!mapped()) {
        bool b = Platform::Mem::unmap_mem(mapped_area, get_size());
        if (!b)
            ABORT_WITH("Unable to unmap region");
    }
}

void
Model::SimpleAS::flush_callback(Vbus::Bus::DeviceEntry* de, const VcpuCtx*) {
    Vbus::Device* dev = de->device;

    if (dev->type() == Vbus::Device::GUEST_PHYSICAL_STATIC_MEMORY || dev->type() == Vbus::Device::GUEST_PHYSICAL_DYNAMIC_MEMORY) {
        Model::SimpleAS* as = reinterpret_cast<Model::SimpleAS*>(dev);

        as->flush_guest_as();
    }
}

char*
Model::SimpleAS::gpa_to_vmm_view(GPA addr, size_t sz) const {
    if (!is_gpa_valid(addr, sz))
        return nullptr;

    if (!mapped())
        return nullptr;

    mword off = addr.get_value() - _as.begin();

    return _vmm_view + off;
}

Model::SimpleAS*
Model::SimpleAS::get_as_device_at(const Vbus::Bus& bus, GPA addr, size_t sz) {
    Vbus::Device* dev = bus.get_device_at(addr.get_value(), sz);

    if (__UNLIKELY__(dev == nullptr
                     || (dev->type() != Vbus::Device::GUEST_PHYSICAL_DYNAMIC_MEMORY
                         && dev->type() != Vbus::Device::GUEST_PHYSICAL_STATIC_MEMORY))) {
        return nullptr;
    }

    return static_cast<Model::SimpleAS*>(dev);
}

char*
Model::SimpleAS::gpa_to_vmm_view(const Vbus::Bus& bus, GPA addr, size_t sz) {
    const Model::SimpleAS* tgt = get_as_device_at(bus, addr, sz);
    if (tgt == nullptr)
        return nullptr;

    return tgt->gpa_to_vmm_view(addr, sz);
}

Errno
Model::SimpleAS::read_bus(const Vbus::Bus& bus, GPA addr, char* dst, size_t sz) {
    Model::SimpleAS* tgt = get_as_device_at(bus, addr, sz);
    if (tgt == nullptr)
        return Errno::INVAL;

    return tgt->read(dst, sz, addr);
}

Errno
Model::SimpleAS::demand_map_bus(const Vbus::Bus& bus, const GPA& gpa, size_t size_bytes, void*& va, bool write) {
    Model::SimpleAS* tgt = get_as_device_at(bus, gpa, size_bytes);
    if (tgt == nullptr)
        return Errno::INVAL;

    void* temp_va = nullptr;
    Errno err = tgt->demand_map(gpa, size_bytes, temp_va, write);

    if (Errno::NONE == err) {
        va = temp_va;
    }

    return err;
}

Errno
Model::SimpleAS::demand_unmap_bus(const Vbus::Bus& bus, const GPA& gpa, size_t size_bytes, void* va) {
    Model::SimpleAS* tgt = get_as_device_at(bus, gpa.value(), size_bytes);
    if (tgt == nullptr)
        return Errno::INVAL;

    return tgt->demand_unmap(gpa, size_bytes, va);
}

Errno
Model::SimpleAS::demand_unmap_bus_clean(const Vbus::Bus& bus, const GPA& gpa, size_t size_bytes, void* va) {
    Model::SimpleAS* tgt = get_as_device_at(bus, gpa.value(), size_bytes);
    if (tgt == nullptr)
        return Errno::INVAL;

    return tgt->demand_unmap_clean(gpa, size_bytes, va);
}

Errno
Model::SimpleAS::write_bus(const Vbus::Bus& bus, GPA addr, const char* src, size_t sz) {
    Model::SimpleAS* tgt = get_as_device_at(bus, addr, sz);
    if (tgt == nullptr)
        return Errno::INVAL;

    return tgt->write(addr, sz, src);
}

bool
Model::SimpleAS::map_host() {
    _vmm_view = reinterpret_cast<char*>(
        Platform::Mem::map_mem(_mobject, 0, _as.size(),
                               Platform::Mem::READ | (_mobject.cred().write() ? Platform::Mem::WRITE : 0), get_mem_fd().msel()));
    return (_vmm_view != nullptr);
}

bool
Model::SimpleAS::destruct() {
    if (mapped()) {
        _vmm_view = nullptr;
        return Platform::Mem::unmap_mem(reinterpret_cast<void*>(_vmm_view), _as.size());
    } else {
        return true;
    }
}

char*
Model::SimpleAS::map_guest_mem(const Vbus::Bus& bus, GPA gpa, size_t sz, bool write) {
    Model::SimpleAS* tgt = get_as_device_at(bus, gpa, sz);
    if (tgt == nullptr) {
        WARN("Cannot map guest memory pa:0x%llx size:0x%lx. Memory range doesn't exist", gpa.get_value(), sz);
        return nullptr;
    }

    if (write && tgt->is_read_only()) {
        WARN("Cannot map read-only guest memory for write pa:0x%llx size:0x%lx", gpa.get_value(), sz);
        return nullptr;
    }

    mword offset = gpa.get_value() - tgt->get_guest_view().get_value();
    DEBUG("map_guest_mem pa:0x%llx size:0x%lx write:%d (+0x%lx)", gpa.get_value(), sz, write, offset);
    void* dst = tgt->map_view(offset, sz, write);
    if (dst == nullptr) {
        WARN("Unable to map a chunk pa:%llx size:0x%lx", gpa.get_value(), sz);
    }

    /* keep track of mapped areas. add to RangeMap */
    return reinterpret_cast<char*>(dst);
}

void
Model::SimpleAS::unmap_guest_mem(const void* mem, size_t sz) {
    /* unmap memory */
    DEBUG("unmap_guest_mem mem:0x%p size:0x%lx", mem, sz);
    bool b = Platform::Mem::unmap_mem(mem, sz);
    if (!b)
        ABORT_WITH("Unable to unmap guest memory mem:0x%p size:0x%lx", mem, sz);
}

void
Model::SimpleAS::lookup_mem_ranges(const Vbus::Bus& bus, const Range<uint64>& gpa_range, Vector<Model::SimpleAS*>& out) {
    out.reset();
    auto* dev_p = SimpleAS::get_as_device_at(bus, GPA{gpa_range.begin()}, gpa_range.size());
    if (dev_p != nullptr) {
        out.push_back(dev_p);
    }
}
