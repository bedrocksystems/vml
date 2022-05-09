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
Model::SimpleAS::single_access_read(uint64 off, uint8 size) const {
    ASSERT(off % size == 0);
    ASSERT(size <= sizeof(uint64));

    uint64 ret;
    void* ptr;
    if (!mapped()) {
        ptr = map_view(off, size, true);
    } else {
        ptr = get_vmm_view() + off;
    }

    ASSERT(ptr != nullptr);

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

    if (!mapped()) {
        unmap_guest_mem(ptr, size);
    }
    return ret;
}

void
Model::SimpleAS::single_access_write(uint64 off, uint8 size, uint64 value) const {
    ASSERT(off % size == 0);
    ASSERT(size <= sizeof(uint64));

    void* ptr;
    if (!mapped()) {
        ptr = map_view(off, size, true);
    } else {
        ptr = get_vmm_view() + off;
    }

    ASSERT(ptr != nullptr);

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

    icache_sync_range(ptr, size);
    if (!mapped()) {
        unmap_guest_mem(ptr, size);
    }
}
#pragma GCC diagnostic pop

Errno
Model::SimpleAS::read(char* dst, size_t size, const GPA& addr) const {
    if (!is_gpa_valid(addr, size))
        return EINVAL;

    mword offset = addr.get_value() - get_guest_view().get_value();
    void* src;

    if (!mapped()) {
        src = Platform::Mem::map_mem(_mobject, offset, size, Platform::Mem::READ);
        if (src == nullptr)
            return ENOMEM;
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
    return ENONE;
}

Errno
Model::SimpleAS::write(const GPA& gpa, size_t size, const char* src) const {
    if (!is_gpa_valid(gpa, size))
        return EINVAL;

    mword offset = gpa.get_value() - get_guest_view().get_value();
    void* dst;

    if (!mapped()) {
        dst = Platform::Mem::map_mem(_mobject, offset, size,
                                     Platform::Mem::READ | Platform::Mem::WRITE);
        if (dst == nullptr)
            return ENOMEM;
    } else {
        dst = get_vmm_view() + offset;
    }

    memcpy(dst, src, size);
    dcache_clean_range(dst, size);
    icache_invalidate_range(dst, size);

    if (!mapped()) {
        bool b = Platform::Mem::unmap_mem(dst, size);
        if (!b)
            ABORT_WITH("Unable to unmap region");
    }

    return ENONE;
}

void*
Model::SimpleAS::map_view(mword offset, size_t size, bool write) const {
    if (_read_only && write && !_mobject.cred().write())
        return nullptr;

    void* dst = Platform::Mem::map_mem(_mobject, offset, size,
                                       Platform::Mem::READ | (write ? Platform::Mem::WRITE : 0));
    if (dst == nullptr)
        ABORT_WITH("Unable to map view of the guest region:0x%llx offset:0x%lx "
                   "size:0x%lx",
                   get_guest_view().get_value(), offset, size);

    return dst;
}

Errno
Model::SimpleAS::clean_invalidate(GPA gpa, size_t size) const {
    if (!is_gpa_valid(gpa, size))
        return EINVAL;

    mword offset = gpa.get_value() - get_guest_view().get_value();
    void* dst;

    if (!mapped()) {
        dst = Platform::Mem::map_mem(_mobject, offset, size,
                                     Platform::Mem::READ | Platform::Mem::WRITE);
        if (dst == nullptr)
            return ENOMEM;
    } else {
        dst = get_vmm_view() + offset;
    }

    dcache_clean_invalidate_range(dst, size);

    if (!mapped()) {
        bool b = Platform::Mem::unmap_mem(dst, size);
        if (!b)
            ABORT_WITH("Unable to unmap region");
    }
    return ENONE;
}

void
Model::SimpleAS::flush_guest_as() {
    if (_read_only or not _flushable or not _mobject.cred().write())
        return;

    void* mapped_area;
    if (!mapped()) {
        mapped_area = Platform::Mem::map_mem(_mobject, 0, get_size(),
                                             Platform::Mem::READ | Platform::Mem::WRITE);
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

    if (dev->type() == Vbus::Device::GUEST_PHYSICAL_STATIC_MEMORY
        || dev->type() == Vbus::Device::GUEST_PHYSICAL_DYNAMIC_MEMORY) {
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
        return EINVAL;

    return tgt->read(dst, sz, addr);
}

Errno
Model::SimpleAS::write_bus(const Vbus::Bus& bus, GPA addr, const char* src, size_t sz) {
    Model::SimpleAS* tgt = get_as_device_at(bus, addr, sz);
    if (tgt == nullptr)
        return EINVAL;

    return tgt->write(addr, sz, src);
}

Errno
Model::SimpleAS::clean_invalidate_bus(const Vbus::Bus& bus, GPA addr, size_t sz) {
    Model::SimpleAS* tgt = get_as_device_at(bus, addr, sz);
    if (tgt == nullptr)
        return EINVAL;

    return tgt->clean_invalidate(addr, sz);
}

bool
Model::SimpleAS::construct(GPA guest_base, const mword size, bool map) {
    _as = Range<mword>(guest_base.get_value(), size);
    if (!map) {
        return true;
    }

    _vmm_view = reinterpret_cast<char*>(Platform::Mem::map_mem(
        _mobject, 0, size,
        Platform::Mem::READ | (_mobject.cred().write() ? Platform::Mem::WRITE : 0)));
    return (_vmm_view != nullptr);
}

bool
Model::SimpleAS::destruct() {
    bool b = true;
    if (mapped()) {
        b = Platform::Mem::unmap_mem(reinterpret_cast<void*>(_vmm_view), _as.size());
        _vmm_view = nullptr;
    }
    return b;
}

char*
Model::SimpleAS::map_guest_mem(const Vbus::Bus& bus, GPA gpa, size_t sz, bool write) {
    Model::SimpleAS* tgt = get_as_device_at(bus, gpa, sz);
    if (tgt == nullptr) {
        WARN("Cannot map guest memory pa:0x%llx size:0x%lx. Memory range doesn't exist",
             gpa.get_value(), sz);
        return nullptr;
    }

    if (write && tgt->is_read_only()) {
        WARN("Cannot map read-only guest memory for write pa:0x%llx size:0x%lx", gpa.get_value(),
             sz);
        return nullptr;
    }

    mword offset = gpa.get_value() - tgt->get_guest_view().get_value();
    DEBUG("map_guest_mem pa:0x%llx size:0x%lx write:%d (+0x%lx)", gpa.get_value(), sz, write,
          offset);
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

struct AsLookupArg {
    Range<uint64> range;
    Vector<Model::SimpleAS*>* out;
};

void
add_as_range(Vbus::Bus::DeviceEntry* de, AsLookupArg* arg) {
    if (de->device->type() != Vbus::Device::GUEST_PHYSICAL_STATIC_MEMORY
        && de->device->type() != Vbus::Device::GUEST_PHYSICAL_DYNAMIC_MEMORY) {
        return;
    }
    Model::SimpleAS* as = reinterpret_cast<Model::SimpleAS*>(de->device);
    Range<uint64> devr{as->get_guest_view().get_value(), as->get_size()};
    if (devr.intersect(arg->range))
        arg->out->insert(as);
}

void
Model::SimpleAS::lookup_mem_ranges(Vbus::Bus& bus, const Range<uint64>& gpa_range,
                                   Vector<Model::SimpleAS*>& out) {

    out.reset();
    AsLookupArg arg{gpa_range, &out};
    bus.iter_devices(add_as_range, &arg);
}
