/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <intrusive/map.hpp>
#include <msr/msr_id.hpp>
#include <vbus/vbus.hpp>

namespace Msr {

    class RegisterBase;
    class Register;
    class BaseBus;

    using Vbus::Err;
}

class Msr::RegisterBase : public Map_key<mword> {
public:
    RegisterBase(const char* name, Id reg_id) : Map_key(), _name(name), _reg_id(reg_id) {}

    virtual Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, uint64& res) = 0;

    uint32 id() const { return _reg_id.id(); };

    /*! \brief Reset the device to its initial state
     *  \pre The caller has full ownership of a valid Device object which can be in any state.
     *  \post The ownership of the object is returned to the caller. The device is in its initial
     *        state.
     */
    virtual void reset(const VcpuCtx* vcpu_ctx) = 0;

    /*! \brief Query the name of the device
     *  \pre The caller has partial ownership of a valid Device object
     *  \post A pointer pointing to a valid array of chars representing the name of the this device.
     *        The caller also receives a fractional ownership of the name.
     *  \return the name of the device
     */
    const char* name() const { return _name; }

private:
    const char* _name; /*!< Name of the device - cannot be changed at run-time */
    Id _reg_id;
};

class Msr::Register : public RegisterBase {
protected:
    uint64 _value;
    uint64 _reset_value;

private:
    uint64 const _write_mask;
    bool const _writable;

public:
    Register(const char* name, Id const reg_id, bool const writable, uint64 const reset_value, uint64 const mask = ~0ULL)
        : RegisterBase(name, reg_id), _value(reset_value), _reset_value(reset_value), _write_mask(mask), _writable(writable) {}

    Err access(Vbus::Access access, const VcpuCtx*, uint64& value) override {
        if (access == Vbus::WRITE && !_writable)
            return Err::ACCESS_ERR;

        if (access == Vbus::WRITE) {
            _value |= value & _write_mask;                    // Set the bits at 1
            _value &= ~(_write_mask & (value ^ _write_mask)); // Set the bits at 0
        } else {
            value = _value;
        }

        return Err::OK;
    }

    void reset(const VcpuCtx*) override { _value = _reset_value; }
};

/*
 * This is the bus that will handle all reads and writes
 * to system registers.
 */
class Msr::BaseBus {
public:
    BaseBus() {}

    /*! \brief Add a register to the msr bus
     *  \pre Full ownership of a valid virtual bus. Full ownership of a valid register.
     *  \post Ownership of the bus is unchanged. The bus adds this register to its
     *        internal list if there is no conflict. Otherwise, ownership of the register
     *        is returned and no changes are performed on the bus.
     *  \param r RegisterBase to add
     *  \return true if there is no conflict and the register was added. false otherwise.
     */
    [[nodiscard]] bool register_device(RegisterBase* r, mword id);

    /*! \brief Query for a register with given id
     *  \pre Fractional ownership of a valid msr bus.
     *  \post Ownership of the bus is unchanged. The bus itself is not changed.
     *        If a register was found, a fractional ownership to a valid RegisterBase is returned.
     *  \param id Id of the device in the Bus
     *  \return nullptr is no register with that id, the register otherwise.
     */
    RegisterBase* get_device_at(mword id) const { return _devices[id]; }

    /*! \brief Access the register at the given location
     *  \pre Fractional ownership of a valid bus. Full ownership of a valid Vcpu context.
     *  \post Ownership of the bus is unchanged. Ownership of the Vcpu context is returned.
     *        If a register was found at the given range, access was called on that register.
     *        An status is returned to understand the consequence of the call.
     *  \param access Type of access (R/W/X)
     *  \param vcpu_ctx Contains information about the VCPU generating this access
     *  \param id Id of the device in the Bus
     *  \param val Buffer for read or write operations
     *  \return The status of the access
     */
    Err access(Vbus::Access access, const VcpuCtx& vcpu_ctx, mword id, uint64& val);

    /*! \brief Reset all registers on the bus
     *  \pre Fractional ownership of a valid msr bus.
     *  \post Ownership of the bus is unchanged. All registers must have transitioned from some
     *        state to their initial state.
     */
    void reset(const VcpuCtx& vcpu_ctx);

    /*! \brief Debug only: control the trace of the access to the bus
     *  \param enabled Should accesses be traced?
     *  \param fold_successive Should repeated accesses to the same register be logged only once?
     */
    void set_trace(bool enabled, bool fold_successive) {
        _trace = enabled;
        _fold = fold_successive;
    }

    RegisterBase* get_register_with_id(Msr::Id id) const { return reinterpret_cast<RegisterBase*>(get_device_at(id.id())); }

private:
    static void reset_register_cb(Msr::RegisterBase*, const VcpuCtx*);

    void log_trace_info(const RegisterBase* reg, Vbus::Access access, uint64 val);

    Map_kv<mword, RegisterBase> _devices;
    bool _trace{false};
    bool _fold{true};
    const RegisterBase* _last_access{nullptr};
    size_t _num_accesses{0};

protected:
    bool register_system_reg(RegisterBase* reg) {
        ASSERT(reg != nullptr);
        if (reg == nullptr)
            return false;
        bool ret;

        ret = register_device(reg, reg->id());
        if (!ret) {
            Msr::RegisterBase* r = get_register_with_id(reg->id());
            if (r != nullptr) {
                ABORT_WITH("Trying to register %s, but, ID is used by %s", reg->name(), r->name());
            } else {
                ABORT_WITH("Unable to register %s, allocation failure", reg->name());
            }
        }

        return true;
    }
};
