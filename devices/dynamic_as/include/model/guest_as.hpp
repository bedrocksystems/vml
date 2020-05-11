/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <model/page_info.hpp>
#include <model/simple_as.hpp>
#include <platform/log.hpp>
#include <platform/rangemap.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Zeta {
    class Zeta_ctx;
};

namespace Model {
    class Guest_as;
};

class Model::Guest_as : public Model::Simple_as {
public:
    enum As_data_id {
        AS_DATA_FDT = 0,
        AS_DATA_KERNEL,
        AS_DATA_PAYLOAD,
        AS_DATA_MAX,
    };

    const char *as_data_name[AS_DATA_MAX] = {"FDT", "Kernel", "Payload"};

    Guest_as(bool read_only) : Model::Simple_as(read_only) {}

    uint64 get_size() const { return _as.size(); }
    bool set_guest_as(const mword guest_base, const mword size, const mword vmm_off = mword(0));

    bool add_data(As_data_id id, Range<mword> &loc, const void *data);
    bool is_data_valid(As_data_id id) const {
        ASSERT(id < AS_DATA_MAX);
        return _data_ranges[id].data != nullptr && _data_ranges[id].loc.size() != 0;
    }

    bool is_gpa_valid(GPA addr) const { return _as.in_range(addr.get_value()); }

    GPA get_guest_view() const { return GPA(_as.begin()); }
    GPA get_guest_data_start(As_data_id id) const;
    GPA get_guest_data_end(As_data_id id) const;

    mword get_data_off_end(As_data_id id) const;

    char *get_vmm_view_of_data(As_data_id id) const;
    char *get_vmm_view() const { return _vmm_view; };
    char *gpa_to_vmm_view(GPA addr) const;
    Tsc setup_guest_as() const;

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx *vcpu_ctx, mword off, uint8 bytes,
                             uint64 &res) override;

    virtual void reset() override {
        flush_guest_as();
        setup_guest_as();
    }

    virtual Type type() override { return GUEST_PHYSICAL_DYNAMIC_MEMORY; }

    /*! \brief Query the current set of permissions for gpa
     *  \pre Fractional owner ship of the Guest_As and a guarantee that all VCPUs are stopped.
     *  \post If the range is valid, permissions on the pages are changed.
     *        Ownership on the Guest_As is unchanged. VCPUs can resume after the call.
     *  \param gpa Guest physical address that will be queried for permissions
     *  \return The permission on the pages. It is assumed by this function that the
     *   gpa given as a parameter is valid. If not, the return value will be irrelevant.
     *   Note that the result of this query is only guaranteed to be stable if all VCPUs
     *   are stopped.
     */
    Page_permission get_perm_for_page(GPA gpa);

    /*! \brief Set the permission for the given gpa
     *  \pre Fractional owner ship of the Guest_As and a guarantee that all VCPUs are stopped.
     *       TODO: we may also need a lock to guarantee that only one thread can proceed here.
     *       The VCPUs tokens are not saying anything about external threads.
     *  \post If the range is valid, permissions on the pages are changed.
     *        Ownership on the Guest_As is unchanged. VCPUs can resume after the call.
     *  \param start First guest physical address for which permissions will change
     *  \param size Size in bytes - covers the range of pages that will be updated.
     *  \param perm the new permissions that will be applied
     *
     *  If the gpa is not valid, no permission will be updated. This function should
     * only be called when VCPUs are stopped. start will be rounded down to the page
     * boundary and size will be rounded up (to cover a full range of pages).
     */
    Errno set_perm_for_range(const Zeta::Zeta_ctx *ctx, GPA start, uint64 size,
                             Page_permission perm);

private:
    void flush_guest_as_data(As_data_id id) const;
    void patch_guest_fdt() const;
    uint64 gpa_to_page_idx(GPA addr) const;

    Page_permission *_perms;

    // Describe data inside _as
    struct As_data {
        Range<mword> loc;          // Range(guest address (gpa), number of bytes to copy).
        const void *data{nullptr}; // host address (hva) for the location of bytes to copy.
    };

    As_data _data_ranges[AS_DATA_MAX];
};
