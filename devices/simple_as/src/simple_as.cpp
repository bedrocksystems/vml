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
Model::Simple_as::read(char* dst, size_t size, GPA& addr) {
    if (!is_gpa_valid(addr, size))
        return EINVAL;
    mword offset = addr.get_value() - get_guest_view().get_value();
    memcpy(dst, get_vmm_view() + offset, size);
    return ENONE;
}

Errno
Model::Simple_as::write(GPA& gpa, size_t size, const char* src) {
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
Model::Simple_as::flush_guest_as() const {
    if (!_read_only) {
        dcache_clean_range(_vmm_view, _as.size());
        icache_invalidate_range(_vmm_view, _as.size());
    }
}

void
Model::Simple_as::flush_callback(Vbus::Bus::Device_entry* de, void*) {
    Vbus::Device* dev = de->device;

    if (dev->type() == Vbus::Device::GUEST_PHYSICAL_STATIC_MEMORY
        || dev->type() == Vbus::Device::GUEST_PHYSICAL_DYNAMIC_MEMORY) {
        Model::Simple_as* as = reinterpret_cast<Model::Simple_as*>(dev);

        as->flush_guest_as();
    }
}

char*
Model::Simple_as::gpa_to_vmm_view(GPA addr, size_t sz) const {
    if (!is_gpa_valid(addr, sz))
        return nullptr;

    mword off = addr.get_value() - _as.begin();

    return _vmm_view + off;
}

char*
Model::Simple_as::gpa_to_vmm_view(const Vbus::Bus& bus, GPA addr, size_t sz) {
    const Vbus::Device* dev = bus.get_device_at(addr.get_value(), sz);

    if (__UNLIKELY__(dev == nullptr
                     || (dev->type() != Vbus::Device::GUEST_PHYSICAL_DYNAMIC_MEMORY
                         && dev->type() != Vbus::Device::GUEST_PHYSICAL_STATIC_MEMORY))) {
        return nullptr;
    }

    static const Model::Simple_as* tgt = reinterpret_cast<const Model::Simple_as*>(dev);
    return tgt->gpa_to_vmm_view(addr, sz);
}
