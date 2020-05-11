/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/*! \file Basic Address Space representation of the guest memory
 */

#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/rangemap.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Simple_as;
};

/*! \brief Guest Physical Address
 */
class GPA {
public:
    /*! \brief Construct a GPA based on a uint64 value
     *  \pre Nothing
     *  \post Full ownership of a valid GPA with the value given as a parameter
     */
    explicit GPA(uint64 v) : _value(v) {}

    /*! \brief Return an integer (uint64) representing the guest address
     *  \pre Partial ownership of a valid GPA
     *  \post Ownership unchanged, a uint64 value matching the internal representation is returned
     */
    uint64 get_value() const { return _value; }

private:
    uint64 _value;
};

/*! \brief Simple (static) Address space representation for the guest
 *
 * This class is a virtual device and is meant to be plugged into a virtual bud.
 * This is useful to perform address space operations from the bus. An example
 * is flushing the whole AS of the guest.
 */
class Model::Simple_as : public Vbus::Device {
public:
    /*! \brief Construct a Simple AS
     *  \pre Nothing
     *  \post Full ownership of Simple AS. The Vbus::Device is initialized and read_only is stored.
     *  \param read_only is the AS read-only from the guest point of view?
     */
    Simple_as(bool read_only) : Vbus::Device("SimpleAS"), _read_only(read_only) {}

    /*! \brief Get the size of this AS
     *  \pre Partial ownership of this object
     *  \post Ownership unchanged. The stored size is returned.
     *  \return the size of the AS
     */
    uint64 get_size() const { return _as.size(); }

    /*! \brief Set the parameters of the AS (size, mapping, base guest address)
     *  \pre Full ownership of the object.
     *  \post Ownership unchanged. The AS info are stored in the object
     *  \param guest_base Address of the mapping from the guest point (guest physical)
     *  \param size Size of the address space
     *  \param vmm_off Offset between guest_base and the vmm mapping of the guest AS
     */
    void set_guest_as(const mword guest_base, const mword size, const mword vmm_off = mword(0)) {
        _as = Range<mword>(guest_base, size);
        _vmm_view = reinterpret_cast<char *>(guest_base + vmm_off);
    }

    /*! \brief Set the parameters of the AS (size, mapping, base guest address)
     *  \pre Full ownership of the object.
     *  \post Ownership unchanged. The AS info are stored in the object
     *  \param guest_base Address of the mapping from the guest point (guest physical)
     *  \param size Size of the address space
     *  \param addr Address where the guest AS is mapped for the current process
     */
    void set_guest_as(const mword guest_base, const mword size, char *addr) {
        _as = Range<mword>(guest_base, size);
        _vmm_view = addr;
    }

    /*! \brief Is the given GPA valid in this AS?
     *  \pre Partial ownership of the object.
     *  \post Ownership unchanged. true if the address belongs to this AS, false otherwise.
     *  \param addr Guest physical address to test
     *  \return true if the address belongs to this AS, false otherwise.
     */
    bool is_gpa_valid(GPA addr) { return _as.contains(addr.get_value()); }

    /*! \brief Query the base of the address space from the guest point of view
     *  \pre Partial ownership of the object.
     *  \post Ownership unchanged.
     *  \return GPA representing the base of this AS
     */
    GPA get_guest_view() const { return GPA(_as.begin()); }

    /*! \brief Query the base of the address space from the VMM point of view
     *  \pre Partial ownership of the object.
     *  \post Ownership unchanged.
     *  \return Address representing the beginning of the mapping of the guest AS
     */
    char *get_vmm_view() const { return _vmm_view; };

    /*! \brief Read data from the guest AS
     *  \pre Partial ownership of the object. Full ownership of the destination buffer.
     *  \post Ownership unchanged. The data is copied from the guest AS to the buffer if the
     * parameters given by the caller are valid. Otherwise, the buffer is left untouched.
     *  \param dst buffer that will receive the guest data
     *  \param size size to read
     *  \param addr start of the read on the guest AS
     *  \return ENONE if the operation was a success, error code otherwise
     */
    Errno read(char *dst, size_t size, GPA &addr);

    /*! \brief Write data to the guest AS
     *  \pre Partial ownership of the object. Full ownership of the source buffer.
     *  \post Ownership unchanged. The data is copied from the buffer to the guest AS if the
     * parameters given by the caller are valid. Otherwise, the guest AS is left untouched.
     *  \param addr start of the write on the guest AS
     *  \param size size to write
     *  \param src buffer that contains the data to write
     *  \return ENONE if the operation was a success, error code otherwise
     */
    Errno write(GPA &addr, size_t size, char *src);

    /*! \brief Callback that can be used to iterate over flushable AS in a virtual Bus
     *  \pre Nothing
     *  \post If the device is an address space, the flush function is called.
     *  \param de current device when iterating over the devices in a bus
     */
    static void flush_callback(Vbus::Bus::Device_entry *de, void *);

    /*! \brief Return the type of this device
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged
     *  \return GUEST_PHYSICAL_STATIC_MEMORY
     */
    virtual Type type() override { return GUEST_PHYSICAL_STATIC_MEMORY; }

    /*! \brief Access function inherited from the parent class
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged
     *  \return Vbus::ACCESS_ERR
     *  \note This function can only be called in case of a guest page fault. But, this
     *        address space being static, this function shouldn't be called.
     */
    virtual Vbus::Err access(Vbus::Access, const Vcpu_ctx *, mword, uint8, uint64 &) override {
        return Vbus::ACCESS_ERR;
    }

    /*! \brief Reset the AS - nothing is performed for now
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged
     */
    virtual void reset() override {}

protected:
    /*! \brief Iterate over this AS and make sure that all data made it to physical RAM
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged
     */
    void flush_guest_as() const;

    const bool _read_only;    /*!< Is the AS read-only from the guest point of view? */
    char *_vmm_view{nullptr}; /*!< base host mapping of base gpa. */
    Range<mword> _as;         /*!< Range(gpa RAM base, guest RAM size) */
};
