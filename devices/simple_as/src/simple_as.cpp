/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <arch/mem_util.hpp>
#include <model/simple_as.hpp>
#include <platform/string.hpp>

Errno
Model::SimpleAS::read(char* dst, size_t size, const GPA& addr) const {
    if (!is_gpa_valid(addr, size))
        return EINVAL;
    mword offset = addr.get_value() - get_guest_view().get_value();
    memcpy(dst, get_vmm_view() + offset, size);
    return ENONE;
}

Errno
Model::SimpleAS::write(GPA& gpa, size_t size, const char* src) const {
    mword offset;
    void* hva = nullptr;

    if (!is_gpa_valid(gpa, size))
        return EINVAL;

    offset = gpa.get_value() - get_guest_view().get_value();
    hva = get_vmm_view() + offset;

    memcpy(hva, src, size);

    dcache_clean_range(hva, size);
    icache_invalidate_range(hva, size);

    return ENONE;
}

void
Model::SimpleAS::flush_guest_as() const {
    if (!_read_only) {
        dcache_clean_range(_vmm_view, _as.size());
        icache_invalidate_range(_vmm_view, _as.size());
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

    mword off = addr.get_value() - _as.begin();

    return _vmm_view + off;
}

static const Model::SimpleAS*
get_as_device_at(const Vbus::Bus& bus, GPA addr, size_t sz) {
    const Vbus::Device* dev = bus.get_device_at(addr.get_value(), sz);

    if (__UNLIKELY__(dev == nullptr
                     || (dev->type() != Vbus::Device::GUEST_PHYSICAL_DYNAMIC_MEMORY
                         && dev->type() != Vbus::Device::GUEST_PHYSICAL_STATIC_MEMORY))) {
        return nullptr;
    }

    return static_cast<const Model::SimpleAS*>(dev);
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
    const Model::SimpleAS* tgt = get_as_device_at(bus, addr, sz);
    if (tgt == nullptr)
        return EINVAL;

    return tgt->read(dst, sz, addr);
}

Errno
Model::SimpleAS::write_bus(const Vbus::Bus& bus, GPA addr, const char* src, size_t sz) {
    const Model::SimpleAS* tgt = get_as_device_at(bus, addr, sz);
    if (tgt == nullptr)
        return EINVAL;

    return tgt->write(addr, sz, src);
}
