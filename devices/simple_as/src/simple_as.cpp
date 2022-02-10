/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <arch/mem_util.hpp>
#include <model/simple_as.hpp>

#include <platform/mem.hpp>
#include <platform/string.hpp>

Errno
Model::SimpleAS::read(char* dst, size_t size, const GPA& addr) const {
    if (!is_gpa_valid(addr, size))
        return EINVAL;

    mword offset = addr.get_value() - get_guest_view().get_value();
    void* src;

    if (!_mapped) {
        src = Platform::Mem::map_mem(_mobject, offset, size, Platform::Mem::READ);
        if (src == nullptr)
            return ENOMEM;
    } else {
        src = get_vmm_view() + offset;
    }

    memcpy(dst, src, size);

    /* unmap */
    if (!_mapped) {
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

    if (!_mapped) {
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

    if (!_mapped) {
        bool b = Platform::Mem::unmap_mem(dst, size);
        if (!b)
            ABORT_WITH("Unable to unmap region");
    }

    return ENONE;
}

void*
Model::SimpleAS::map_view(mword offset, size_t size, bool write) const {
    if (_read_only && write)
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

    if (!_mapped) {
        dst = Platform::Mem::map_mem(_mobject, offset, size,
                                     Platform::Mem::READ | Platform::Mem::WRITE);
        if (dst == nullptr)
            return ENOMEM;
    } else {
        dst = get_vmm_view() + offset;
    }

    dcache_clean_invalidate_range(dst, size);

    if (!_mapped) {
        bool b = Platform::Mem::unmap_mem(dst, size);
        if (!b)
            ABORT_WITH("Unable to unmap region");
    }
    return ENONE;
}

void
Model::SimpleAS::flush_guest_as() {
    if (_read_only)
        return;

    void* mapped_area;
    if (!_mapped) {
        mapped_area = Platform::Mem::map_mem(_mobject, 0, _as.size(),
                                             Platform::Mem::READ | Platform::Mem::WRITE);
        if (mapped_area == nullptr)
            ABORT_WITH("Unable to map guest region %llx", get_guest_view().get_value());
    } else {
        mapped_area = _vmm_view;
    }

    dcache_clean_range(mapped_area, _as.size());
    icache_invalidate_range(mapped_area, _as.size());

    if (!_mapped) {
        bool b = Platform::Mem::unmap_mem(mapped_area, _as.size());
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

    if (!_mapped)
        return nullptr;

    mword off = addr.get_value() - _as.begin();

    return _vmm_view + off;
}

static Model::SimpleAS*
get_as_device_at(const Vbus::Bus& bus, GPA addr, size_t sz) {
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
        _mobject, 0, size, Platform::Mem::READ | (!_read_only ? Platform::Mem::WRITE : 0)));
    if (_vmm_view == nullptr) {
        return false;
    }

    _mapped = true;

    return true;
}

bool
Model::SimpleAS::destruct() {
    bool b = true;
    if (_mapped) {
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
