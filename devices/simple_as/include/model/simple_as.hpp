/*
 * Copyright (C) 2020-2022 BedRock Systems, Inc.
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
#include <platform/vector.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class SimpleAS;
};

/*! \brief Page permission data type
 *
 * This enum represents page permissions as bitflags.
 */
enum class PagePermission : uint8 {
    NONE = 0,
    READ = (1 << 0),
    WRITE = (1 << 1),
    EXEC_USER = (1 << 2),
    EXEC_SUPERVISOR = (1 << 3),
    EXEC = EXEC_USER | EXEC_SUPERVISOR,
    READ_WRITE = READ | WRITE,
    READ_EXEC_USER = READ | EXEC_USER,
    READ_EXEC_SUPERVISOR = READ | EXEC_SUPERVISOR,
    READ_EXEC = READ | EXEC,
    WRITE_EXEC_USER = WRITE | EXEC_USER,
    WRITE_EXEC_SUPERVISOR = WRITE | EXEC_SUPERVISOR,
    WRITE_EXEC = WRITE | EXEC,
    READ_WRITE_EXEC_USER = READ | WRITE | EXEC_USER,
    READ_WRITE_EXEC_SUPERVISOR = READ | WRITE | EXEC_SUPERVISOR,
    READ_WRITE_EXEC = READ | WRITE | EXEC,
};

inline const char *
page_permission_to_str(PagePermission p) {
    switch (p) {
    case PagePermission::NONE:
        return "---";
    case PagePermission::READ:
        return "R--";
    case PagePermission::WRITE:
        return "-W-";
    case PagePermission::EXEC_USER:
        return "--XU";
    case PagePermission::EXEC_SUPERVISOR:
        return "--XS";
    case PagePermission::EXEC:
        return "--X";
    case PagePermission::READ_WRITE:
        return "RW-";
    case PagePermission::READ_EXEC_USER:
        return "R-XU";
    case PagePermission::READ_EXEC_SUPERVISOR:
        return "R-XS";
    case PagePermission::READ_EXEC:
        return "R-X";
    case PagePermission::WRITE_EXEC_USER:
        return "-WXU";
    case PagePermission::WRITE_EXEC_SUPERVISOR:
        return "-WXS";
    case PagePermission::WRITE_EXEC:
        return "-WX";
    case PagePermission::READ_WRITE_EXEC_USER:
        return "RWXU";
    case PagePermission::READ_WRITE_EXEC_SUPERVISOR:
        return "RWXS";
    case PagePermission::READ_WRITE_EXEC:
        return "RWX";
    default:
        return "UNKNOWN";
    }
}

/*! \brief binary or operator on PagePermission
 *  \pre None
 *  \post returns binary or of input permissions
 */
inline PagePermission
operator|(PagePermission a, PagePermission b) {
    return static_cast<PagePermission>(static_cast<uint8>(a) | static_cast<uint8>(b));
}

/*! \brief binary and operator on PagePermission
 *  \pre None
 *  \post returns binary and of input permissions
 */
inline PagePermission
operator&(PagePermission a, PagePermission b) {
    return static_cast<PagePermission>(static_cast<uint8>(a) & static_cast<uint8>(b));
}

/*! \brief binary xor operator on PagePermission
 *  \pre None
 *  \post returns binary xor of input permissions
 */
inline PagePermission
operator^(PagePermission a, PagePermission b) {
    return static_cast<PagePermission>(static_cast<uint8>(a) ^ static_cast<uint8>(b));
}

/*! \brief binary or assignment operator on PagePermission
 *  \pre None
 *  \post returns binary or of input permissions and sets this value in the first parameter
 */
inline PagePermission &
operator|=(PagePermission &a, PagePermission b) {
    return (a = a | b);
}

/*! \brief binary and assignment operator on PagePermission
 *  \pre None
 *  \post returns binary and of input permissions and sets this value in the first parameter
 */
inline PagePermission &
operator&=(PagePermission &a, PagePermission b) {
    return (a = a & b);
}

/*! \brief binary xor assignment operator on PagePermission
 *  \pre None
 *  \post returns binary xor of input permissions and sets this value in the first parameter
 */
inline PagePermission &
operator^=(PagePermission &a, PagePermission b) {
    return (a = a ^ b);
}

/*! \brief binary not operator on PagePermission
 *  \pre None
 *  \post returns binary not of input permission
 */
inline PagePermission
operator~(PagePermission a) {
    return static_cast<PagePermission>((~static_cast<uint8>(a)) & 0xf);
}

/*! \brief function to determine whether read is set in an instance of PagePermission
 *  \pre None
 *  \post returns true if read is set, otherwise false
 */
inline bool
pp_is_read_set(PagePermission a) {
    return static_cast<bool>(a & PagePermission::READ);
}

/*! \brief function to determine whether write is set in an instance of PagePermission
 *  \pre None
 *  \post returns true if write is set, otherwise false
 */
inline bool
pp_is_write_set(PagePermission a) {
    return static_cast<bool>(a & PagePermission::WRITE);
}

/*! \brief function to determine whether exec (user or supervisor) is set in an instance
           of PagePermission
 *  \pre None
 *  \post returns true if exec user or supervisor is set, otherwise false
 */
inline bool
pp_is_exec_set(PagePermission a) {
    return static_cast<bool>(a & PagePermission::EXEC);
}

/*! \brief function to determine whether exec user is set in an instance
           of PagePermission
 *  \pre None
 *  \post returns true if exec user is set, otherwise false
 */
inline bool
pp_is_exec_user_set(PagePermission a) {
    return static_cast<bool>(a & PagePermission::EXEC_USER);
}

/*! \brief function to determine whether exec supervisor is set in an instance
           of PagePermission
 *  \pre None
 *  \post returns true if exec supervisor is set, otherwise false
 */
inline bool
pp_is_exec_supervisor_set(PagePermission a) {
    return static_cast<bool>(a & PagePermission::EXEC_SUPERVISOR);
}

/*! \brief Simple Wrapper for primitive types.
 */
template<typename T>
class PrimitiveTypeWrapper {
public:
    PrimitiveTypeWrapper() = delete;
    /*
     * We ignore the coding style for this constructor. At the moment, VMI
     * is converting from uint64 to GPA freely. We can address that but it
     * requires a bit of surgery.
     */
    PrimitiveTypeWrapper(T value) : _value(value) {} // NOLINT
    PrimitiveTypeWrapper(const PrimitiveTypeWrapper &other) : _value(other._value) {}

    void set_value(T value) { _value = value; }
    T get_value() const { return _value; }
    T value() const { return _value; }

    // Operator overloading
    bool operator==(const T &value) const { return value == _value; }
    bool operator!=(const T &value) const { return value != _value; }
    bool operator==(const PrimitiveTypeWrapper &other) const { return _value == other._value; }
    bool operator!=(const PrimitiveTypeWrapper &other) const { return _value != other._value; }
    PrimitiveTypeWrapper &operator=(const T value) {
        _value = value;
        return *this;
    }
    PrimitiveTypeWrapper &operator=(const PrimitiveTypeWrapper &other) {
        _value = other._value;
        return *this;
    }
    T operator()(void) const { return _value; }
    explicit operator const T &() const { return _value; }

    PrimitiveTypeWrapper &operator+=(const T &other) {
        _value += other;
        return *this;
    }

    PrimitiveTypeWrapper &operator+=(const PrimitiveTypeWrapper &other) {
        _value += other._value;
        return *this;
    }

    PrimitiveTypeWrapper &operator-=(const T &other) {
        _value -= other;
        return *this;
    }

    PrimitiveTypeWrapper &operator-=(const PrimitiveTypeWrapper &other) {
        _value -= other._value;
        return *this;
    }

    PrimitiveTypeWrapper &operator&=(const T &other) {
        _value &= other;
        return *this;
    }

    PrimitiveTypeWrapper &operator&=(const PrimitiveTypeWrapper &other) {
        _value &= other._value;
        return *this;
    }

    PrimitiveTypeWrapper &operator|=(const T &other) {
        _value |= other;
        return *this;
    }

    PrimitiveTypeWrapper &operator|=(const PrimitiveTypeWrapper &other) {
        _value |= other._value;
        return *this;
    }

    bool operator<=(const PrimitiveTypeWrapper &other) const { return _value <= other._value; }
    bool operator<=(const T other) const { return _value <= other; }
    friend bool operator<=(T number, const PrimitiveTypeWrapper &prim) {
        return number <= prim._value;
    }

    bool operator>=(const PrimitiveTypeWrapper &other) const { return _value >= other._value; }
    bool operator>=(const T other) const { return _value >= other; }
    friend bool operator>=(T number, const PrimitiveTypeWrapper &prim) {
        return number >= prim._value;
    }

    bool operator<(const PrimitiveTypeWrapper &other) const { return _value < other._value; }
    bool operator<(const T other) const { return _value < other; }
    friend bool operator<(T number, const PrimitiveTypeWrapper &prim) {
        return number < prim._value;
    }

    bool operator>(const PrimitiveTypeWrapper &other) const { return _value > other._value; }
    bool operator>(const T other) const { return _value > other; }
    friend bool operator>(T number, const PrimitiveTypeWrapper &prim) {
        return number > prim._value;
    }

    T operator+(const PrimitiveTypeWrapper<T> &other) const { return _value + other._value; }
    T operator+(const T value) const { return _value + value; }

    T operator-(const PrimitiveTypeWrapper<T> &other) const { return _value - other._value; }
    T operator-(const T value) const { return _value - value; }

    T operator&(const PrimitiveTypeWrapper<T> &other) const { return _value & other._value; }
    T operator&(const T value) const { return _value & value; }

    T operator%(const PrimitiveTypeWrapper<T> &other) const { return _value % other._value; }
    T operator%(const T value) const { return _value % value; }

protected:
    T _value;
};

/*! \brief Guest Physical Address
 */
class GPA : public PrimitiveTypeWrapper<uint64> {
public:
    static constexpr uint64 INVALID_GPA = ~0ull;
    static constexpr uint64 INVALID_GFN = ~0ull;
    using PrimitiveTypeWrapper::PrimitiveTypeWrapper;

    GPA() : PrimitiveTypeWrapper<uint64>(INVALID_GPA) {}

    /**
     * \brief Get the guest frame number.
     */
    uint64 gfn(void) const { return (_value >> PAGE_BITS); }

    /**
     * \brief Check whether the GPA is invalid.
     *
     * \return true The GPA is invalid.
     * \return false The GPA is valid.
     */
    bool invalid(void) const { return _value == INVALID_GPA; }

    /**
     * \brief Convert a GFN to a GPA.
     *
     * \param gfn The GFN to convert.
     * \return GPA The resulting GPA.
     */
    static GPA gfn_to_gpa(uint64 gfn) { return GPA(gfn << PAGE_BITS); }
};

/*! \brief Hooks for (effectful) GPA translation and un-translation.
 *
 * The intended usage is:
 * | char *va = nullptr;
 * | Errno err;
 * |
 * | err = translator->gpa_to_va_{write}(addr, sz, va);
 * | if (Errno::ENONE != err) return err;
 * |
 * | // use [va] in a readable XOR writable way
 * |
 * | err = translator->gpa_to_va_post_{write}(addr, sz, va);
 * | if (Errno::ENONE != err) return err;
 */
class GuestPhysicalToVirtual {
public:
    virtual ~GuestPhysicalToVirtual() {}

    // Address translation for readable and writable chunks of memory might
    // differ. Therefore, [GuestPhysicalToVirtual] exposes pre-/post-hooks
    // which are explicitly slated for use in readable and writable translations:
    // - [gpa_to_va]/[gpa_to_va_post]: pre-/post-hooks for *readable*
    //   guest-physical ranges.
    // - [gpa_to_va_write]/[gpa_to_va_post_write]: pre-/post-hooks for *writable*
    //   guest-physical ranges.
    //
    // Libraries shall use the appropriate parity when translating guest-phsyical
    // addresses using this interface.
    //
    // Clients which are indifferent to the read/write parity may simply override
    // [gpa_to_va] (and [gpa_to_va_post], if necessary).
    virtual Errno gpa_to_va(const GPA &gpa, size_t byte_size, char *&va) = 0;
    virtual Errno gpa_to_va_write(const GPA &gpa, size_t byte_size, char *&va) {
        return gpa_to_va(gpa, byte_size, va);
    }
    virtual Errno gpa_to_va_post(const GPA &gpa, size_t byte_size, char *va) {
        (void)(gpa);
        (void)(byte_size);
        (void)(va);
        return ENONE;
    }
    virtual Errno gpa_to_va_post_write(const GPA &gpa, size_t byte_size, char *va) {
        return gpa_to_va_post(gpa, byte_size, va);
    }
};

/*! \brief Guest Virtual Address
 */
class GVA : public PrimitiveTypeWrapper<mword> {
public:
    static constexpr mword INVALID_GVA = ~0ull;
    using PrimitiveTypeWrapper::PrimitiveTypeWrapper;

    GVA() : PrimitiveTypeWrapper<mword>(INVALID_GVA) {}

    /**
     * \brief Get the guest page number (virtual address shifted by page bits)
     */
    mword gpn(void) const { return (_value >> PAGE_BITS); }

    /**
     * \brief Check whether the GVA is invalid.
     *
     * \return true The GVA is invalid.
     * \return false The GVA is valid.
     */
    bool invalid(void) const { return _value == INVALID_GVA; }

    /**
     * \brief Convert a GPN to a GVA.
     *
     * \param gpn The gpn to convert to a GVA.
     * \return GVA The resulting GVA.
     */
    static GVA gpn_to_gva(mword gpn) { return GVA(gpn << PAGE_BITS); }
};

/*! \brief Host Virtual Address
 */
class HVA : public PrimitiveTypeWrapper<mword> {
public:
    static constexpr mword INVALID_HVA = ~0ull;
    using PrimitiveTypeWrapper::PrimitiveTypeWrapper;
};

/*! \brief Simple (static) Address space representation for the guest
 *
 * This class is a virtual device and is meant to be plugged into a virtual bud.
 * This is useful to perform address space operations from the bus. An example
 * is flushing the whole AS of the guest.
 */
class Model::SimpleAS : public Vbus::Device {
public:
    /*! \brief Construct a Simple AS
     *  \pre Nothing
     *  \post Full ownership of Simple AS. The Vbus::Device is initialized and read_only is stored.
     *  \param read_only is the AS read-only from the guest point of view?
     */
    explicit SimpleAS(const Platform::Mem::MemDescr &descr, bool read_only, bool flushable = true)
        : Vbus::Device("SimpleAS"), _read_only(read_only), _flushable(flushable), _mobject(descr) {}

    /*! \brief Construct a Simple AS
     *  \pre Gives up ownership of the name string
     *  \post Full ownership of Simple AS. The Vbus::Device is initialized and read_only is stored.
     *  \param name name of the virtual device
     *  \param read_only is the AS read-only from the guest point of view?
     */
    SimpleAS(const char *name, const Platform::Mem::MemDescr &descr, bool read_only,
             bool flushable = true)
        : Vbus::Device(name), _read_only(read_only), _flushable(flushable), _mobject(descr) {}

    bool construct(GPA guest_base, size_t size, bool map);
    bool destruct();

    /*! \brief Get the size of this AS
     *  \pre Partial ownership of this object
     *  \post Ownership unchanged. The stored size is returned.
     *  \return the size of the AS
     */
    uint64 get_size() const { return _as.size(); }

    /*! \brief Get BHV selector for a memory object for this AS
     *  \pre Partial ownership of this object
     *  \post Ownership unchanged. Selector of memory object is returned.
     *  \return the selector of memory object
     */
    const Platform::Mem::MemDescr &get_mem_fd() const { return _mobject; }

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
    Errno read(char *dst, size_t size, const GPA &addr) const;

    Errno demand_map(const GPA &gpa, size_t size_bytes, void *&va, bool write) const;
    Errno demand_unmap(const GPA &gpa, size_t size_bytes, void *va) const;
    Errno demand_unmap_clean(const GPA &gpa, size_t size_bytes, void *va) const;

    static Errno demand_map_bus(const Vbus::Bus &bus, const GPA &gpa, size_t size_bytes, void *&va,
                                bool write);
    static Errno demand_unmap_bus(const Vbus::Bus &bus, const GPA &gpa, size_t size_bytes,
                                  void *va);
    static Errno demand_unmap_bus_clean(const Vbus::Bus &bus, const GPA &gpa, size_t size_bytes,
                                        void *va);

public:
    /*! \brief Write data to the guest AS
     *  \pre Partial ownership of the object. Full ownership of the source buffer.
     *  \post Ownership unchanged. The data is copied from the buffer to the guest AS if the
     * parameters given by the caller are valid. Otherwise, the guest AS is left untouched.
     *  \param gpa start of the write on the guest AS
     *  \param size size to write
     *  \param src buffer that contains the data to write
     *  \return ENONE if the operation was a success, error code otherwise
     */
    Errno write(const GPA &gpa, size_t size, const char *src) const;

    /*! \brief Callback that can be used to iterate over flushable AS in a virtual Bus
     *  \pre Nothing
     *  \post If the device is an address space, the flush function is called.
     *  \param de current device when iterating over the devices in a bus
     */
    static void flush_callback(Vbus::Bus::DeviceEntry *de, const VcpuCtx *);

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
    virtual Vbus::Err access(Vbus::Access, const VcpuCtx *, Vbus::Space, mword, uint8,
                             uint64 &) override {
        return Vbus::ACCESS_ERR;
    }

    /*! \brief Reset the AS - nothing is performed for now
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged
     */
    virtual void reset(const VcpuCtx *) override {}

    /*! \brief Converts a GPA to an address valid for the VMM
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged. It the GPA is within this AS, a valid pointer to memory. nullptr
     *        otherwise.
     *  \param addr the Guest Physical address to convert
     *  \param sz Size of the access
     *  \return A valid pointer to memory if GPA is valid within this AS. nullptr otherwise.
     */
    char *gpa_to_vmm_view(GPA addr, size_t sz) const;

    void *map_view(mword offset, size_t size, bool write) const;

    bool is_read_only() const { return _read_only; }

    static char *gpa_to_vmm_view(const Vbus::Bus &bus, GPA addr, size_t sz);
    static char *map_guest_mem(const Vbus::Bus &bus, GPA gpa, size_t sz, bool write);
    static void unmap_guest_mem(const void *mem, size_t sz);

    static Model::SimpleAS *get_as_device_at(const Vbus::Bus &bus, GPA addr, size_t sz);

    static Errno read_bus(const Vbus::Bus &bus, GPA addr, char *dst, size_t sz);
    static Errno write_bus(const Vbus::Bus &bus, GPA addr, const char *src, size_t sz);
    static Errno clean_invalidate_bus(const Vbus::Bus &bus, GPA addr, size_t sz);

    static void lookup_mem_ranges(const Vbus::Bus &bus, const Range<uint64> &gpa_range,
                                  Vector<Model::SimpleAS *> &out);

protected:
    uint64 single_access_read(uint64 off, uint8 size) const;
    void single_access_write(uint64 off, uint8 size, uint64 value) const;

    /*! \brief Iterate over this AS and make sure that all data made it to physical RAM
     *  \pre Partial ownership of this device
     *  \post Ownership unchanged
     */
    void flush_guest_as();
    Errno clean_invalidate(GPA gpa, size_t size) const;
    bool mapped() const { return (_vmm_view != nullptr); }

    const bool _read_only;    /*!< Is the AS read-only from the guest point of view? */
    const bool _flushable;    /*!< Are full AS flush operations needed/allowed? */
    char *_vmm_view{nullptr}; /*!< base host mapping of base gpa. */
    Range<mword> _as;         /*!< Range(gpa RAM base, guest RAM size) */

    Platform::Mem::MemDescr _mobject; /*!< BHV Memory Range object behind this guest range */
};

class MappingGuard {
public:
    MappingGuard(const Vbus::Bus &bus, const GPA &gpa, size_t size_bytes)
        : _bus(&bus), _gpa(gpa), _size_bytes(size_bytes), _va(nullptr) {}

    Errno map(void *&va, bool write = false) {
        Errno err = Model::SimpleAS::demand_map_bus(*_bus, _gpa, _size_bytes, va, write);
        if (err == ENONE)
            _va = va;
        return err;
    }

    ~MappingGuard() {
        if (_va) {
            Model::SimpleAS::demand_unmap_bus(*_bus, _gpa, _size_bytes, _va);
        }
    }

private:
    const Vbus::Bus *_bus;
    const GPA _gpa;
    size_t _size_bytes;
    void *_va;
};
