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
#include <zeta/ec.hpp>

Errno
Model::SimpleAS::read_mapped(char* dst, size_t size, const GPA& addr) const {
    if (!is_gpa_valid(addr, size))
        return EINVAL;

    mword offset = addr.get_value() - get_guest_view().get_value();
    memcpy(dst, get_vmm_view() + offset, size);
    dcache_clean_range(get_vmm_view() + offset, size);
    icache_invalidate_range(get_vmm_view() + offset, size);
    return ENONE;
}

Errno
Model::SimpleAS::read(char* dst, size_t size, const GPA& addr) {
    if (!is_gpa_valid(addr, size))
        return EINVAL;

    Platform::MutexGuard guard(&_mapping_lock);
    Errno err = vmm_mmap(Zeta::Ec::ctx(), addr, size, false, true);
    if (err != ENONE)
        return err;

    err = Model::SimpleAS::read_mapped(dst, size, addr);
    if (err != ENONE)
        return err;

    err = vmm_mmap(Zeta::Ec::ctx(), addr, size, false, false);
    if (err != ENONE)
        return err;

    return ENONE;
}

Errno
Model::SimpleAS::write(GPA& gpa, size_t size, const char* src) {
    if (!is_gpa_valid(gpa, size))
        return EINVAL;

    Platform::MutexGuard guard(&_mapping_lock);
    Errno err = vmm_mmap(Zeta::Ec::ctx(), gpa, size, true, true);
    if (err != ENONE)
        return err;

    err = Model::SimpleAS::write_mapped(gpa, size, src);
    if (err != ENONE)
        return err;

    err = vmm_mmap(Zeta::Ec::ctx(), gpa, size, false, false);
    if (err != ENONE)
        return err;

    return ENONE;
}

Errno
Model::SimpleAS::write_mapped(GPA& gpa, size_t size, const char* src) const {
    void* hva = gpa_to_vmm_view(gpa, size);
    if (hva == nullptr)
        return EINVAL;

    memcpy(hva, src, size);

    dcache_clean_range(hva, size);
    icache_invalidate_range(hva, size);

    return ENONE;
}

Errno
Model::SimpleAS::clean_invalidate(GPA addr, size_t sz) {
    void* hva = gpa_to_vmm_view(addr, sz);
    if (hva == nullptr)
        return EINVAL;

    Platform::MutexGuard guard(&_mapping_lock);
    Errno err = vmm_mmap(Zeta::Ec::ctx(), addr, sz, true, true);
    if (err != ENONE)
        return err;

    dcache_clean_invalidate_range(hva, sz);

    err = vmm_mmap(Zeta::Ec::ctx(), addr, sz, false, false);
    if (err != ENONE)
        return err;

    return ENONE;
}

void
Model::SimpleAS::flush_guest_as() {
    if (_flushable) {
        Platform::MutexGuard guard(&_mapping_lock);

        INFO("Flushing @ 0x%llx", get_guest_view().get_value());
        for (uint64 i = 0; i < (get_size() / PAGE_SIZE); i++) {
            if ((get_guest_view() + i * PAGE_SIZE) < 0x90000000)
                continue;

            Errno err = vmm_mmap(Zeta::Ec::ctx(), get_guest_view() + i * PAGE_SIZE, PAGE_SIZE, true,
                                 true);
            ASSERT(err == ENONE);
            dcache_clean_range(_vmm_view + i * PAGE_SIZE, PAGE_SIZE);
            icache_sync_range(_vmm_view + i * PAGE_SIZE, PAGE_SIZE);
            err = vmm_mmap(Zeta::Ec::ctx(), get_guest_view() + i * PAGE_SIZE, PAGE_SIZE, false,
                           false);
            ASSERT(err == ENONE);
        }
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
Model::SimpleAS::vmm_mmap(const Zeta::Zeta_ctx* ctx, GPA start, uint64 size, bool will_write,
                          bool map) const {
    if (!_is_nested)
        return Errno::ENONE;
    if (get_guest_view().get_value() == 0x17e2c0000) // Keep hypervisor pagetables mapped for now
        return ENONE;

    GPA first_page(align_dn(start.get_value(), PAGE_SIZE));
    uint64 size_roundedup
        = align_up(size + (first_page.get_value() - start.get_value()), PAGE_SIZE);
    Range<mword> perm_range(first_page.get_value(), max<mword>(PAGE_SIZE, size_roundedup));

    char* first_vmm_page = gpa_to_vmm_view(GPA(perm_range.begin()), perm_range.size());
    if (__UNLIKELY__(first_vmm_page == nullptr)) {
        WARN("Invalid range [0x%lx:0x%lx] - cannot update permissions", perm_range.begin(),
             perm_range.last());
        return EINVAL;
    }

    bool read, write;
    if (map) {
        read = true;
        write = will_write;
    } else {
        ASSERT(!will_write);
        read = false;
        write = false;
    }

    // INFO("Permissions at [0x%lx:0x%lx] updated to R:%u W:%u", perm_range.begin(),
    // perm_range.last(),
    //     read, write);
    Nova::MemCred cred(read, write, false);
    Errno err
        = Zeta::mmap_update(ctx, first_vmm_page, perm_range.size(), cred, Nova::Table::CPU_HST);
    if (err != ENONE) {
        WARN("mmap update failure with %u @ [0x%lx:0x%lx] - cannot update permissions", err,
             perm_range.begin(), perm_range.last());
        return err;
    }

    return ENONE;
}

Errno
Model::SimpleAS::clean_invalidate_bus(const Vbus::Bus& bus, GPA addr, size_t sz) {
    Model::SimpleAS* tgt = get_as_device_at(bus, addr, sz);
    if (tgt == nullptr)
        return EINVAL;

    return tgt->clean_invalidate(addr, sz);
}
