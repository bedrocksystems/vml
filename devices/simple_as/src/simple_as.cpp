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
Model::SimpleAS::map_view(void* dst, mword offset, size_t size, bool write) const {
    if (_read_only && write)
        return EINVAL;

    auto map_area = Range<mword>{reinterpret_cast<mword>(dst), size}.aligned_expand(PAGE_BITS);

    Errno errno = BHV::MemRange::map(Zeta::Ec::ctx(), _mobject, map_area.begin() >> PAGE_BITS,
                                     map_area.size() >> PAGE_BITS, offset >> PAGE_BITS,
                                     BHV::MemRange::READ | (write ? BHV::MemRange::WRITE : 0),
                                     Nova::Table::CPU_HST);
    if (errno != ENONE)
        ABORT_WITH("Unable to map view of the guest dst:0x%llx region:0x%llx offset:0x%llx "
                   "size:0x%llx: err:%d",
                   dst, get_guest_view().get_value(), offset, size, errno);

    return errno;
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

char*
Model::SimpleAS::map_guest_mem(const Vbus::Bus& bus, GPA gpa, size_t sz, bool write) {
    Model::SimpleAS* tgt = get_as_device_at(bus, gpa, sz);
    if (tgt == nullptr) {
        WARN("Cannot map guest memory pa:0x%llx size:0x%llx. Memory range doesn't exist",
             gpa.get_value(), sz);
        return nullptr;
    }

    if (write && tgt->is_read_only()) {
        WARN("Cannot map read-only guest memory for write pa:0x%llx size:0x%llx", gpa.get_value(),
             sz);
        return nullptr;
    }

    /* allocate memory */
    auto map_area = Range<mword>{static_cast<mword>(gpa.get_value()), sz}.aligned_expand(PAGE_BITS);
    char* dst = reinterpret_cast<char*>(Vmap::pagealloc(numpages(map_area.size())));
    if (dst == nullptr) {
        ABORT_WITH("Unable to allocate memory ");
        return nullptr;
    }

    mword offset = map_area.begin() - tgt->get_guest_view().get_value();
    INFO("map_guest_mem pa:0x%llx size:0x%llx write:%d to 0x%llx (+0x%lx) offset:0x%lx "
         "area_size:0x%lx",
         gpa.get_value(), sz, write, dst, (gpa.get_value() & ~PAGE_MASK), offset, map_area.size());
    Errno err = tgt->map_view(dst, offset, map_area.size(), write);
    if (err != ENONE) {
        Vmap::free(dst, map_area.size());
        WARN("Unable to map a chunk pa:%llx size:0x%llx", gpa.get_value(), sz);
        return nullptr;
    }

    /* keep track of mapped areas. add to RangeMap */
    return dst + (gpa.get_value() & ~PAGE_MASK);
}

void
Model::SimpleAS::unmap_guest_mem(const void* mem, size_t sz) {
    /* unmap memory */
    INFO("unmap_guest_mem mem:0x%llx size:0x%llx", mem, sz);
    auto map_area = Range<mword>{reinterpret_cast<mword>(mem), sz}.aligned_expand(PAGE_BITS);
    Errno err = Zeta::munmap(Zeta::Ec::ctx(), reinterpret_cast<void*>(map_area.begin()),
                             map_area.size(), Nova::Table::CPU_HST);
    if (err != ENONE) {
        ABORT_WITH("Unable to unmap guest memory mem:0x%llx size:0x%llx err:%d", mem, sz, err);
    }
    /* XXX: free memory range form range map if we track mapped regions */
    Vmap::free(const_cast<void*>(mem), sz);
}
