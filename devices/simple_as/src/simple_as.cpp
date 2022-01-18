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

#include <bhv/bhv.hpp>

Errno
Model::SimpleAS::read(char* dst, size_t size, const GPA& addr) const {
    if (!is_gpa_valid(addr, size))
        return EINVAL;

    Platform::MutexGuard guard(&_mapping_lock);

    mword offset = addr.get_value() - get_guest_view().get_value();
    char* src = get_vmm_view() + offset;

    auto map_area = Range<mword>{reinterpret_cast<mword>(src), size}.aligned_expand(PAGE_BITS);

    Errno errno = BHV::MemRange::map(Zeta::Ec::ctx(), _mobject, map_area.begin() >> PAGE_BITS,
                                     map_area.size() >> PAGE_BITS, offset >> PAGE_BITS,
                                     BHV::MemRange::READ, Nova::Table::CPU_HST);

    if (errno != ENONE)
        return errno;

    memcpy(dst, src, size);

    /* unmap */
    errno = Zeta::munmap(Zeta::Ec::ctx(), reinterpret_cast<void*>(map_area.begin()),
                         map_area.size(), Nova::Table::CPU_HST);
    if (errno != ENONE)
        ABORT_WITH("Unable to unmap region");

    return ENONE;
}

Errno
Model::SimpleAS::write(const GPA& gpa, size_t size, const char* src) const {
    if (!is_gpa_valid(gpa, size))
        return EINVAL;

    Platform::MutexGuard guard(&_mapping_lock);

    mword offset = gpa.get_value() - get_guest_view().get_value();
    char* hva = get_vmm_view() + offset;

    auto map_area = Range<mword>{reinterpret_cast<mword>(hva), size}.aligned_expand(PAGE_BITS);

    Errno errno = BHV::MemRange::map(Zeta::Ec::ctx(), _mobject, map_area.begin() >> PAGE_BITS,
                                     map_area.size() >> PAGE_BITS, offset >> PAGE_BITS,
                                     BHV::MemRange::WRITE, Nova::Table::CPU_HST);
    if (errno != ENONE)
        return errno;

    memcpy(hva, src, size);
    dcache_clean_range(hva, size);
    /* XXX: it will call Model::Cpu::ctrl_feature_on_all_vcpus */
    icache_invalidate_range(hva, size);

    /* unmap */
    errno = Zeta::munmap(Zeta::Ec::ctx(), reinterpret_cast<void*>(map_area.begin()),
                         map_area.size(), Nova::Table::CPU_HST);
    if (errno != ENONE)
        ABORT_WITH("Unable to unmap region");

    return ENONE;
}

Errno
Model::SimpleAS::clean_invalidate(GPA gpa, size_t size) const {
    if (!is_gpa_valid(gpa, size))
        return EINVAL;

    Platform::MutexGuard guard(&_mapping_lock);

    mword offset = gpa.get_value() - get_guest_view().get_value();
    char* hva = get_vmm_view() + offset;

    auto map_area = Range<mword>{reinterpret_cast<mword>(hva), size}.aligned_expand(PAGE_BITS);

    Errno errno = BHV::MemRange::map(Zeta::Ec::ctx(), _mobject, map_area.begin() >> PAGE_BITS,
                                     map_area.size() >> PAGE_BITS, offset >> PAGE_BITS,
                                     BHV::MemRange::WRITE, Nova::Table::CPU_HST);
    if (errno != ENONE)
        return errno;

    dcache_clean_invalidate_range(hva, size);

    /* unmap */
    errno = Zeta::munmap(Zeta::Ec::ctx(), reinterpret_cast<void*>(map_area.begin()),
                         map_area.size(), Nova::Table::CPU_HST);
    if (errno != ENONE)
        ABORT_WITH("Unable to unmap region");

    return ENONE;
}


void
Model::SimpleAS::flush_guest_as() {
    if (_read_only)
        return;
    Platform::MutexGuard guard(&_mapping_lock);

    auto map_area
        = Range<mword>{reinterpret_cast<mword>(_vmm_view), _as.size()}.aligned_expand(PAGE_BITS);
    Errno errno = BHV::MemRange::map(Zeta::Ec::ctx(), _mobject, map_area.begin() >> PAGE_BITS,
                                     map_area.size() >> PAGE_BITS, 0, BHV::MemRange::WRITE,
                                     Nova::Table::CPU_HST);
    if (errno != ENONE)
        ABORT_WITH("Unable to map guest region %llx", get_guest_view().get_value());

    dcache_clean_range(_vmm_view, _as.size());
    icache_invalidate_range(_vmm_view, _as.size());

    /* unmap */
    errno = Zeta::munmap(Zeta::Ec::ctx(), reinterpret_cast<void*>(map_area.begin()),
                         map_area.size(), Nova::Table::CPU_HST);
    if (errno != ENONE)
        ABORT_WITH("Unable to unmap region");
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
    const Model::SimpleAS* tgt = get_as_device_at(bus, addr, sz);
    if (tgt == nullptr)
        return EINVAL;

    return tgt->clean_invalidate(addr, sz);
}
