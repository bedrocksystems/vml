/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <arch/mem_util.hpp>
#include <bedrock/fdt.hpp>
#include <bits.hpp>
#include <debug_switches.hpp>
#include <log/log.hpp>
#include <model/guest_as.hpp>
#include <model/vcpu_types.hpp>
#include <outpost/outpost.hpp>
#include <platform/new.hpp>
#include <vbus/vbus.hpp>
#include <zeta/zeta.hpp>

bool
Model::Guest_as::set_guest_as(const mword guest_base, const mword size, const mword vmm_off) {
    _as = Range<mword>(guest_base, size);
    _vmm_view = reinterpret_cast<char*>(guest_base + vmm_off);

    /*
     * There is a bit of a memory trade-off to consider here. We want to maintain a data structure
     * that keeps track on the permission on every page. For now, I can think of two viable
     * approaches given the current data structures we have.
     *
     * 1 - Keep a range map with ranges of permissions. Memory efficient if we only have a few pages
     * with different permissions. But, not so much when permissions are very diverse across the
     * address space (and ranges cannot be merged). This is also somewhat more complex to write
     * because it involves merging and splitting ranges carefully. Also, it requires dynamic memory
     * and could fail at run time if we cannot allocate Range objects... So, for now, I opted for
     * solution
     *
     * 2 - Allocate a fixed array of bits that maintains the permissions on each page.
     * This is simple but not very memory efficient if all the pages have the same permissions. The
     * overhead is roughly ~100KB per GB of guest memory. However, it has the nice guarantee of
     * being free of runtime errors which is fairly nice and will simplify our APIs. We could change
     * to solution 1 in the future.
     */
    size_t npages = numpages(size);
    ASSERT(_perms == nullptr);
    _perms = new (nothrow) Page_permission[npages];
    if (_perms == nullptr)
        return false;

    return true;
}

bool
Model::Guest_as::add_data(As_data_id id, Range<mword>& loc, const void* data) {
    ASSERT(id < AS_DATA_MAX);

    if (!_as.contains(loc)) {
        WARN("%s [0x%llx:0x%llx] is not in the guest AS [0x%llx:0x%llx]", as_data_name[id],
             loc.start(), loc.last(), _as.begin(), _as.last());
        return false;
    }

    for (size_t i = 0; i < AS_DATA_MAX; ++i) {
        As_data_id cur_id = static_cast<As_data_id>(i);

        if (_data_ranges[i].loc.intersect(loc)) {
            WARN("%s [0x%llx:0x%llx] is overlaping with %s [0x%llx:0x%llx]", as_data_name[id],
                 loc.start(), loc.last(), as_data_name[i], get_guest_data_start(cur_id),
                 get_guest_data_end(cur_id));
            return false;
        }
    }

    _data_ranges[id].loc = loc;
    _data_ranges[id].data = data;

    return true;
}

GPA
Model::Guest_as::get_guest_data_start(As_data_id id) const {
    ASSERT(id < AS_DATA_MAX);
    return GPA(_data_ranges[id].loc.start());
}

GPA
Model::Guest_as::get_guest_data_end(As_data_id id) const {
    ASSERT(id < AS_DATA_MAX);
    return GPA(_data_ranges[id].loc.end());
}

char*
Model::Guest_as::get_vmm_view_of_data(As_data_id id) const {
    ASSERT(id < AS_DATA_MAX);
    mword off = _data_ranges[id].loc.start() - _as.begin();

    return _vmm_view + off;
}

char*
Model::Guest_as::gpa_to_vmm_view(GPA addr) const {
    if (!is_gpa_valid(addr))
        return nullptr;

    mword off = addr.get_value() - _as.begin();

    return _vmm_view + off;
}

uint64
Model::Guest_as::gpa_to_page_idx(GPA addr) const {
    ASSERT(is_gpa_valid(addr));
    return (addr.get_value() - _as.begin()) / PAGE_SIZE;
}

mword
Model::Guest_as::get_data_off_end(As_data_id id) const {
    ASSERT(id < AS_DATA_MAX);
    return _data_ranges[id].loc.end();
}

void
Model::Guest_as::patch_guest_fdt() const {
    Fdt::Tree* tree = new (get_vmm_view_of_data(Model::Guest_as::AS_DATA_FDT)) Fdt::Tree();
    ASSERT(tree->validate() == Fdt::Tree::Format_err::OK);

    Fdt::Prop::Reg_list_iterator mem_entries;
    bool ret = fdt_find_memory(*tree, mem_entries);
    ASSERT(ret);
    ASSERT(mem_entries.num_elements_left() == 1);

    mem_entries.set_address(get_guest_view().get_value());
    mem_entries.set_size(get_size());

    if (is_data_valid(Model::Guest_as::AS_DATA_PAYLOAD)) {
        Fdt::Node chosen(tree->lookup_from_path("/chosen"));
        ASSERT(chosen.is_valid());
        Fdt::Property initrd_start(tree->lookup_property(chosen, "linux,initrd-start"));
        ASSERT(initrd_start.is_valid());
        Fdt::Property initrd_end(tree->lookup_property(chosen, "linux,initrd-end"));
        ASSERT(initrd_end.is_valid());

        GPA start = get_guest_data_start(Model::Guest_as::AS_DATA_PAYLOAD),
            end = get_guest_data_end(Model::Guest_as::AS_DATA_PAYLOAD);

        INFO("Patching guest FDT with initrd-start=0x%llx, initrd-end=0x%llx", start.get_value(),
             end.get_value());

        uint32 written = initrd_start.set_data<uint32>(static_cast<uint32>(start.get_value()));
        ASSERT(written == sizeof(uint32));
        written = initrd_end.set_data<uint32>(static_cast<uint32>(end.get_value()));
        ASSERT(written == sizeof(uint32));
    }

    flush_guest_as_data(Model::Guest_as::AS_DATA_FDT);
}

Tsc
Model::Guest_as::setup_guest_as() const {
    Tsc end_tsc, start_tsc = tsc();

    for (size_t i = 0; i < AS_DATA_MAX; ++i) {
        As_data_id id = static_cast<As_data_id>(i);
        char* dst = get_vmm_view_of_data(id);
        size_t size = _data_ranges[i].loc.size();

        if (!is_data_valid(id))
            continue;

        INFO("Copying %s to the guest AS @ 0x%llx with size 0x%llx", as_data_name[i],
             get_guest_data_start(id).get_value(), size);
        memcpy(dst, _data_ranges[i].data, size);
        flush_data_cache(dst, size);
    }

    if (is_data_valid(AS_DATA_FDT))
        patch_guest_fdt();

    end_tsc = tsc();

    return end_tsc - start_tsc;
}

void
Model::Guest_as::flush_guest_as_data(As_data_id id) const {
    ASSERT(id < AS_DATA_MAX);

    char* dst = get_vmm_view_of_data(id);
    size_t size = _data_ranges[id].loc.size();

    flush_data_cache(dst, size);
}

template<typename T>
uint64
read_from_memory_at_off(const char* data, mword off) {
    T res;

    if (__UNLIKELY__(align_up(off, sizeof(T)) != off)) {
        memcpy(&res, data + off, sizeof(T));
        return res;
    }
/*
 * We disable the warning in this case. The alignment is guaranteed by the runtime check
 * but the compiler doesn't detect that.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    const T* tdata = reinterpret_cast<const T*>(data);
    off /= sizeof(T);
    res = tdata[off];
#pragma GCC diagnostic pop

    return res;
}

template<typename T>
void
write_to_memory_at_off(char* data, mword off, uint64 val) {
    if (__UNLIKELY__(align_up(off, sizeof(T)) != off)) {
        memcpy(data + off, &val, sizeof(T));
        return;
    }
/*
 * We disable the warning in this case. The alignment is guaranteed by the runtime check
 * but the compiler doesn't detect that.
 */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    T* tdata = reinterpret_cast<T*>(data);
    off /= sizeof(T);
    tdata[off] = static_cast<T>(val);
#pragma GCC diagnostic pop
}

Page_permission
Model::Guest_as::get_perm_for_page(GPA gpa) {
    Page_permission ret;

    if (!is_gpa_valid(gpa))
        return Page_permission::NONE;

    ret = _perms[gpa_to_page_idx(gpa)];
    if (Debug::TRACE_PAGE_PERMISSIONS)
        INFO("Permissions at addr 0x%llx are R:%u W:%u X:%u", gpa.get_value(), pp_is_read_set(ret),
             pp_is_write_set(ret), pp_is_exec_set(ret));

    return ret;
}

Errno
Model::Guest_as::set_perm_for_range(const Zeta::Zeta_ctx* ctx, GPA start, uint64 size,
                                    Page_permission perm) {
    GPA first_page(align_dn(start.get_value(), PAGE_SIZE));
    uint64 size_roundedup
        = align_up(size + (first_page.get_value() - start.get_value()), PAGE_SIZE);
    Range<mword> perm_range(first_page.get_value(), max<mword>(PAGE_SIZE, size_roundedup));

    if (!_as.contains(perm_range)) {
        DEBUG("Invalid range [0x%llx:0x%llx] - cannot update permissions", perm_range.begin(),
              perm_range.last());
        return EINVAL;
    }

    char* first_vmm_page = gpa_to_vmm_view(GPA(perm_range.begin()));
    ASSERT(first_vmm_page != nullptr);
    Errno err = Zeta::mmap_update(
        ctx, first_vmm_page, perm_range.size(),
        Nova::Mem_cred(pp_is_read_set(perm), pp_is_write_set(perm), pp_is_exec_set(perm)),
        Nova::MEM_GST);
    if (err != ENONE) {
        DEBUG("mmap update failure with %u @ [0x%llx:0x%llx] - cannot update permissions", err,
              perm_range.begin(), perm_range.last());
        return err;
    }

    for (uint64 idx = gpa_to_page_idx(first_page); idx <= gpa_to_page_idx(GPA(perm_range.last()));
         ++idx) {
        _perms[idx] = perm;
    }

    if (Debug::TRACE_PAGE_PERMISSIONS)
        INFO("Permissions at [0x%llx:0x%llx] updated to R:%u W:%u X:%u", perm_range.begin(),
             perm_range.last(), pp_is_read_set(perm), pp_is_write_set(perm), pp_is_exec_set(perm));
    return ENONE;
}

static inline Page_permission
convert_to_vmi_type(Vbus::Access acc) {
    return acc == Vbus::EXEC ? Page_permission::EXEC :
                               (Vbus::WRITE ? Page_permission::WRITE : Page_permission::READ);
}

Vbus::Err
Model::Guest_as::access(Vbus::Access access, const Vcpu_ctx* vctx, mword off, uint8 bytes,
                        uint64& res) {
    Vmm::Pf::Access_info acc{convert_to_vmi_type(access), get_guest_view().get_value() + off,
                             bytes == Vbus::SIZE_UNKNOWN ? Vmm::Pf::SIZE_INFO_INVALID : bytes};

    outpost::vmi_handle_page_fault(*vctx, acc);

    if (__UNLIKELY__(_read_only && access != Vbus::Access::READ))
        return Vbus::ACCESS_ERR;

    if (Debug::GUEST_MAP_ON_DEMAND) {
        Page_permission cur;

        cur = get_perm_for_page(GPA(acc.gpa));
        if (cur == Page_permission::READ_WRITE_EXEC)
            ABORT_WITH("Page fault on 0x%llx but the page was already faulted in", acc.gpa);

        /*
         * If we end up in this condition, it means the instruction that generated the fault
         * is "complex" and involves large registers of multiple registers. This is not decoded
         * by the caller at the moment. To handle this properly, an instruction emulator is
         * required. We don't have that for now so our only viable option is to fault in the page
         * and replay the instruction. Ideally, we should also single step and restore the previous
         * permission of the page. This will be done later on.
         */
        Errno err
            = set_perm_for_range(vctx->ctx, GPA(acc.gpa), bytes, Page_permission::READ_WRITE_EXEC);
        if (err != ENONE) {
            return Vbus::ACCESS_ERR;
        }

        return Vbus::REPLAY_INST;
    }

    switch (access) {
    case Vbus::Access::READ:
        switch (bytes) {
        case 1:
            res = read_from_memory_at_off<uint8>(get_vmm_view(), off);
            break;
        case 2:
            res = read_from_memory_at_off<uint16>(get_vmm_view(), off);
            break;
        case 4:
            res = read_from_memory_at_off<uint32>(get_vmm_view(), off);
            break;
        case 8:
            res = read_from_memory_at_off<uint64>(get_vmm_view(), off);
            break;
        }
        break;
    case Vbus::Access::WRITE:
        switch (bytes) {
        case 1:
            write_to_memory_at_off<uint8>(get_vmm_view(), off, res);
            break;
        case 2:
            write_to_memory_at_off<uint16>(get_vmm_view(), off, res);
            break;
        case 4:
            write_to_memory_at_off<uint32>(get_vmm_view(), off, res);
            break;
        case 8:
            write_to_memory_at_off<uint64>(get_vmm_view(), off, res);
            break;
        }
        break;
    case Vbus::Access::EXEC:
        /* Pages should have been faulted in at this point.
         */
        return Vbus::REPLAY_INST;
    }

    return Vbus::OK;
}
