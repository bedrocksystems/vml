/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file Basic Address Space representation of the guest memory
 */

#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/memory.hpp>
#include <platform/rangemap.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Simple_as;
};

/*! \brief Page permission data type
 *
 * This enum represents page permissions as bitflags.
 */
enum class Page_permission : uint8 {
    NONE = 0,
    READ = (1 << 0),
    WRITE = (1 << 1),
    EXEC = (1 << 2),
    READ_WRITE = READ | WRITE,
    READ_EXEC = READ | EXEC,
    WRITE_EXEC = WRITE | EXEC,
    READ_WRITE_EXEC = READ | WRITE | EXEC,
};

/*! \brief binary or operator on Page_permission
 *  \pre None
 *  \post returns binary or of input permissions
 */
inline Page_permission
operator|(Page_permission a, Page_permission b) {
    return static_cast<Page_permission>(static_cast<uint8>(a) | static_cast<uint8>(b));
}

/*! \brief binary and operator on Page_permission
 *  \pre None
 *  \post returns binary and of input permissions
 */
inline Page_permission
operator&(Page_permission a, Page_permission b) {
    return static_cast<Page_permission>(static_cast<uint8>(a) & static_cast<uint8>(b));
}

/*! \brief binary xor operator on Page_permission
 *  \pre None
 *  \post returns binary xor of input permissions
 */
inline Page_permission
operator^(Page_permission a, Page_permission b) {
    return static_cast<Page_permission>(static_cast<uint8>(a) ^ static_cast<uint8>(b));
}

/*! \brief binary or assignment operator on Page_permission
 *  \pre None
 *  \post returns binary or of input permissions and sets this value in the first parameter
 */
inline Page_permission &
operator|=(Page_permission &a, Page_permission b) {
    return (a = a | b);
}

/*! \brief binary and assignment operator on Page_permission
 *  \pre None
 *  \post returns binary and of input permissions and sets this value in the first parameter
 */
inline Page_permission &
operator&=(Page_permission &a, Page_permission b) {
    return (a = a & b);
}

/*! \brief binary xor assignment operator on Page_permission
 *  \pre None
 *  \post returns binary xor of input permissions and sets this value in the first parameter
 */
inline Page_permission &
operator^=(Page_permission &a, Page_permission b) {
    return (a = a ^ b);
}

/*! \brief binary not operator on Page_permission
 *  \pre None
 *  \post returns binary not of input permission
 */
inline Page_permission
operator~(Page_permission a) {
    return static_cast<Page_permission>((~static_cast<uint8>(a)) & 0x7);
}

/*! \brief function to determine whether read is set in an instance of Page_permission
 *  \pre None
 *  \post returns true if read is set, otherwise false
 */
inline bool
pp_is_read_set(Page_permission a) {
    return static_cast<bool>(a & Page_permission::READ);
}

/*! \brief function to determine whether write is set in an instance of Page_permission
 *  \pre None
 *  \post returns true if write is set, otherwise false
 */
inline bool
pp_is_write_set(Page_permission a) {
    return static_cast<bool>(a & Page_permission::WRITE);
}

/*! \brief function to determine whether exec is set in an instance of Page_permission
 *  \pre None
 *  \post returns true if exec is set, otherwise false
 */
inline bool
pp_is_exec_set(Page_permission a) {
    return static_cast<bool>(a & Page_permission::EXEC);
}

/*! \brief Simple Wrapper for primitive types.
 */
template<typename T>
class PrimitiveTypeWrapper {
public:
    PrimitiveTypeWrapper() = delete;
    PrimitiveTypeWrapper(T value) : _value(value) {}

    void set_value(T value) { _value = value; }
    T get_value() const { return _value; }

    // Operator overloading
    bool operator==(const T &value) { return value == _value; }
    bool operator!=(const T &value) { return value != _value; }
    bool operator==(const PrimitiveTypeWrapper &other) { return _value == other._value; }
    bool operator!=(const PrimitiveTypeWrapper &other) { return _value != other._value; }
    T &operator=(T value) { _value = value; }
    T operator()(void) const { return _value; }
    operator T &() const { return _value; }

protected:
    T _value;
};

/*! \brief Guest Physical Address
 */
class GPA : public PrimitiveTypeWrapper<uint64> {
public:
    static constexpr uint64 INVALID_GPA = ~0ull;
    using PrimitiveTypeWrapper::PrimitiveTypeWrapper;

    /**
     * \brief Get the guest frame number.
     */
    uint64 gfn(void) { return (_value >> PAGE_BITS); }
};

/*! \brief Guest Virtual Address
 */
class GVA : public PrimitiveTypeWrapper<mword> {
public:
    static constexpr mword INVALID_GVA = ~0ull;
    using PrimitiveTypeWrapper::PrimitiveTypeWrapper;

    /**
     * \brief Get the guest page number (virtual address shifted by page bits)
     */
    mword gpn(void) { return (_value >> PAGE_BITS); }
};

/*! \brief Host Virtual Address
 */
class HVA : public PrimitiveTypeWrapper<mword> {
public:
    static constexpr mword INVALID_GVA = ~0ull;
    using PrimitiveTypeWrapper::PrimitiveTypeWrapper;
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

    /*! \brief Construct a Simple AS
     *  \pre Gives up ownership of the name string
     *  \post Full ownership of Simple AS. The Vbus::Device is initialized and read_only is stored.
     *  \param name name of the virtual device
     *  \param read_only is the AS read-only from the guest point of view?
     */
    Simple_as(const char *name, bool read_only) : Vbus::Device(name), _read_only(read_only) {}

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
     *  \param sz Size of the access
     *  \return true if the address belongs to this AS, false otherwise.
     */
    bool is_gpa_valid(GPA addr, size_t sz) const {
        return _as.contains(Range<mword>(addr.get_value(), sz));
    }

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
    Errno write(GPA &addr, size_t size, const char *src);

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
    virtual Type type() const override { return GUEST_PHYSICAL_STATIC_MEMORY; }

    /*! \brief Access function inherited from the parent class
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged
     *  \return Vbus::ACCESS_ERR
     *  \note This function can only be called in case of a guest page fault. But, this
     *        address space being static, this function shouldn't be called.
     */
    virtual Vbus::Err access(Vbus::Access, const Vcpu_ctx *, Vbus::Space, mword, uint8,
                             uint64 &) override {
        return Vbus::ACCESS_ERR;
    }

    /*! \brief Reset the AS - nothing is performed for now
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged
     */
    virtual void reset(const Vcpu_ctx *) override {}

    /*! \brief Converts a GPA to an address valid for the VMM
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged. It the GPA is within this AS, a valid pointer to memory. nullptr
     *        otherwise.
     *  \param addr the Guest Physical address to convert
     *  \param sz Size of the access
     *  \return A valid pointer to memory if GPA is valid within this AS. nullptr otherwise.
     */
    char *gpa_to_vmm_view(GPA addr, size_t sz) const;

    static char *gpa_to_vmm_view(const Vbus::Bus &bus, GPA addr, size_t sz);

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
