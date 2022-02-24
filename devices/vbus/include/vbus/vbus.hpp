/*
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/rangemap.hpp>
#include <platform/types.hpp>

struct VcpuCtx;

namespace Vbus {

    /*! \brief Error status returned by the Vbus on access operations
     */
    enum Err {
        OK = 0,          /*!< No error */
        REPLAY_INST = 1, /*!< The access was fine but the current instruction was not executed */
        ACCESS_ERR = 2,  /*!< The access was invalid */
        UPDATE_REGISTER = 3, /*!< The access was fine but a register update is required */
        NO_DEVICE = 4        /*!< No device at this address */
    };

    /*! \brief The space that the VBus represents (devices can be added to different spaces)
     */
    enum Space {
        MMIO,           /*!< Devices only, no regular memory */
        REGULAR_MEMORY, /*!< Regular memory only */
        ALL_MEM,        /*!< Everything that is byte-addressable */
        IO_PORT,
        MODEL_SPECIFIC_REGISTER,
        SYSTEM_REGISTER,
        AFFINITY, /*!< Use to look up VCPU based on their affinity */
    };

    enum Access {
        READ = 0,
        WRITE = 1,
        EXEC = 2,
    };

    /*! \brief Special value used by the Vbus caller to specify that the access size is not known
     *
     * In practice, this is used when the VM exit handler in itself does not have enough
     * information to infer the size. Finding out the size will usually require some manual
     * instruction decoding and emulation. This is left to the device for now. A virtual device
     * shouldn't need to worry about this case but it is useful for the virtual RAM.
     */
    static constexpr uint8 SIZE_UNKNOWN = static_cast<uint8>(~0x0);

    class Device;
    class Bus;
}

/*! \brief Abstract device interface - Has to be implemented by all devices connected to the Bus
 */
class Vbus::Device {
public:
    /*! \brief Construct a device with given name
     *  \pre The caller must provide a pointer to a string with at least a fractional ownership
     *  \post A valid Device object is constructed with name set correctly
     *  \param name Name of the device
     */
    explicit Device(const char* name) : _name(name) {}
    virtual ~Device() {}

    /*! \brief Query the name of the device
     *  \pre The caller has partial ownership of a valid Device object
     *  \post A pointer pointing to a valid array of chars representing the name of the this device.
     *        The caller also receives a fractional ownership of the name.
     *  \return the name of the device
     */
    const char* name() const { return _name; }

    /*
     * It is the responsibility of the device to implement these
     * handlers. The off is relative the to the registered range
     * in the bus.
     */
    virtual Err access(Access access, const VcpuCtx* vcpu_ctx, Vbus::Space sp, mword off,
                       uint8 bytes, uint64& res)
        = 0;

    /*! \brief Reset the device to its initial state
     *  \pre The caller has full ownership of a valid Device object which can be in any state.
     *  \post The ownership of the object is returned to the caller. The device is in its initial
     *        state.
     */
    virtual void reset(const VcpuCtx* vcpu_ctx) = 0;

    /*! \brief Type that represents the device
     */
    enum Type {
        DEVICE = 0, /*!< Opaque Device type - cannot be manipulated as a specific device */
        GUEST_PHYSICAL_STATIC_MEMORY,  /*!< Behaves as static physical memory for the guest */
        GUEST_PHYSICAL_DYNAMIC_MEMORY, /*!< Behaves as dynamic physical memory for the guest.
                                          Provides mapping APIs */
        IRQ_CONTROLLER,                /*!< Interrupt Controller */
    };

    /*! \brief Query the type of the device
     *  \pre The caller has partial ownership of a valid Device object
     *  \post The return value contains 'DEVICE'. Ownership and state of the device is unchanged.
     *  \return The 'DEVICE' type
     */
    virtual Type type() const { return DEVICE; }

private:
    const char* _name; /*!< Name of the device - cannot be changed at run-time */
};

/*! \brief Virtual Bus: Represents a hardware. Used to access devices plugged to the VM
 */
class Vbus::Bus {
public:
    /*! \brief Constructs a virtual bus
     *  \param sp Space that this VBus represents
     *  \post Full ownership of the vbus
     */
    explicit Bus(Vbus::Space sp = Space::ALL_MEM, bool absolute_access = false)
        : _space(sp), _devices(), _absolute_access(absolute_access) {}

    /*! \brief Add a device to the virtual bus
     *  \pre Full ownership of a valid virtual bus. Full ownership of a valid Device.
     *  \post Ownership of the vbus is unchanged. The virtual bus adds this device to its
     *        internal list if there is no conflict. Otherwise, ownership of the device
     *        is returned and no changes are performed on the virtual bus.
     *  \param d Device to add
     *  \param addr Location of the device in the Bus. Cannot conflict with another device
     *  \param bytes Range size that device will occupy
     *  \return true if there is no conflict and the device was added. false otherwise.
     */
    [[nodiscard]] bool register_device(Device* d, mword addr, mword bytes);

    /*! \brief Removes a device from the virtual bus
     *  \pre Full ownership of a valid virtual bus. Full ownership of a valid Device.
     *  \post Ownership of the vbus is unchanged. The virtual bus removes this device from its
     *        internal list if it is present and return its full ownership.
     *  \param d Device to remove
     */
    void unregister_device(Device*) {}

    /*! \brief Query for a device that can handle the given range
     *  \pre Fractional ownership of a valid virtual bus.
     *  \post Ownership of the vbus is unchanged. The virtual bus itself is not changed.
     *        If a device was found, a fractional ownership to a valid Device is returned.
     *  \param addr Location of the device in the Bus
     *  \param size Range size that device will occupy
     *  \return nullptr is not device was at this location, the device otherwise.
     */
    Device* get_device_at(mword addr, uint64 size) const;

    /*! \brief Access the device at the given location
     *  \pre Fractional ownership of a valid virtual bus. Full ownership of a valid Vcpu context.
     *  \post Ownership of the vbus is unchanged. Ownership of the Vcpu context is returned.
     *        If a device was found at the given range, access was called on that device.
     *        An status is returned to understand the consequence of the call.
     *  \param access Type of access (R/W/X)
     *  \param vcpu_ctx Contains information about the VCPU generating this access
     *  \param addr Location of the device in the Bus
     *  \param size Size fo the access
     *  \param val Buffer for read or write operations
     *  \return The status of the access
     */
    Err access(Access access, const VcpuCtx& vcpu_ctx, mword addr, uint8 bytes, uint64& val);

    /*! \brief Reset all devices on the bus
     *  \pre Fractional ownership of a valid virtual bus.
     *  \post Ownership of the vbus is unchanged. All devices must have transitioned from some
     *        state to their initial state.
     */
    void reset(const VcpuCtx& vcpu_ctx);

    /*! \brief Debug only: control the trace of the access to the bus
     *  \param enabled Should accesses be traced?
     *  \param fold_successive Should repeated accesses to the same device be logged only once?
     */
    void set_trace(bool enabled, bool fold_successive) {
        _trace = enabled;
        _fold = fold_successive;
    }

    struct DeviceEntry : RangeNode<mword> {
        DeviceEntry(Device* d, const Range<mword>& r) : RangeNode(r), device(d) {}

        Device* device;
    };

    /*! \brief Execute a callback on all the devices of this virtual bus
     *  \pre Fractional ownership of a valid virtual bus.
     *  \post Ownership of the vbus is unchanged. The callback has been called on all devices.
     *  \param f The callback function that will be called on all devices
     *  \param arg the argument that will be passed every time the callback is invoked
     */
    template<typename T>
    void iter_devices(void (*f)(Vbus::Bus::DeviceEntry* de, T*), T* arg) {
        _devices.iter(f, arg);
    }

private:
    static void reset_device_cb(Vbus::Bus::DeviceEntry* entry, const VcpuCtx* arg);

    static void reset_irq_ctlr_cb(Vbus::Bus::DeviceEntry* entry, const VcpuCtx* arg);

    const DeviceEntry* lookup(mword addr, uint64 bytes) const;

    Space _space;
    RangeMap<mword> _devices;
    bool _trace{false};
    bool _fold{true};
    const DeviceEntry* _last_access{nullptr};
    size_t _num_accesses{0};
    bool _absolute_access{false};
};
