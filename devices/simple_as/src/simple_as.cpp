/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <arch/mem_util.hpp>
#include <model/simple_as.hpp>
#include <platform/string.hpp>

Errno
Model::Simple_as::read(char* dst, size_t size, GPA& addr) {
    if (!_as.contains(Range<mword>(addr.get_value(), size))) {
        return EINVAL;
    }
    mword offset = addr.get_value() - get_guest_view().get_value();
    memcpy(dst, get_vmm_view() + offset, size);
    return ENONE;
}

Errno
Model::Simple_as::write(GPA& addr, size_t size, const char* src) {
    if (!_as.contains(Range<mword>(addr.get_value(), size))) {
        return EINVAL;
    }
    mword offset = addr.get_value() - get_guest_view().get_value();
    memcpy(get_vmm_view() + offset, src, size);
    flush_data_cache(get_vmm_view() + offset, size);
    return ENONE;
}

void
Model::Simple_as::flush_guest_as() const {
    flush_data_cache(_vmm_view, _as.size());
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
Model::Simple_as::gpa_to_vmm_view(GPA addr) const {
    if (!is_gpa_valid(addr))
        return nullptr;

    mword off = addr.get_value() - _as.begin();

    return _vmm_view + off;
}