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
Model::Simple_as::write(GPA& addr, size_t size, const char* src) {
    if (!is_gpa_valid(addr, size))
        return EINVAL;
    mword offset = addr.get_value() - get_guest_view().get_value();
    memcpy(get_vmm_view() + offset, src, size);
    icache_sync_range(get_vmm_view() + offset, size);
    return ENONE;
}

void
Model::Simple_as::flush_guest_as() const {
    icache_sync_range(_vmm_view, _as.size());
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
