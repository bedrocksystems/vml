/**
 * Copyright (C) 2019-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <intrusive/map.hpp>
#include <msr/msr_id.hpp>
#include <platform/time.hpp>
#include <vbus/vbus.hpp>

namespace Msr {

    class RegisterBase;
    class Register;
    class BusStats;
    class BaseBus;

    using Vbus::Err;
}

struct MsrStats {
    uint64 read_count{0ull};
    uint64 write_count{0ull};
    Tsc min{~0ull};
    Tsc max{0ull};
    Tsc total{0ull};
};

class Msr::RegisterBase : public MapKey<mword> {
public:
    RegisterBase(const char* name, Id reg_id) : MapKey(), _name(name), _reg_id(reg_id) {}

    virtual Err access(Vbus::Access access, const VcpuCtx* vcpu_ctx, uint64& res) = 0;

    uint32 id() const { return _reg_id.id(); }

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

    Tsc msr_stats_start(Vbus::Access access) {
        if (access == Vbus::WRITE)
            _msr_stats.write_count++;
        if (access == Vbus::READ)
            _msr_stats.read_count++;

        return static_cast<Tsc>(clock());
    }

    void msr_stats_end(Tsc t) {
        Tsc time_spent = static_cast<Tsc>(clock()) - t;
        _msr_stats.total += time_spent;

        if (time_spent < _msr_stats.min)
            _msr_stats.min = time_spent;

        if (time_spent > _msr_stats.max)
            _msr_stats.max = time_spent;
    }

    MsrStats* get_stats() { return &_msr_stats; }

    void reset_stats() {
        _msr_stats.read_count = 0;
        _msr_stats.write_count = 0;
        _msr_stats.min = ~0ull;
        _msr_stats.max = 0;
        _msr_stats.total = 0;
    }

private:
    const char* _name; /*!< Name of the device - cannot be changed at run-time */
    Id _reg_id;

    MsrStats _msr_stats;
};

class Msr::Register : public RegisterBase {
protected:
    uint64 _value;
    uint64 _reset_value;

private:
    uint64 const _write_mask;
    bool const _writable;
    bool const _err_on_write_reserved;

public:
    Register(const char* name, Id const reg_id, bool const writable, uint64 const reset_value, uint64 const mask = ~0ULL,
             bool err_on_write_reserved = false)
        : RegisterBase(name, reg_id), _value(reset_value), _reset_value(reset_value), _write_mask(mask), _writable(writable),
          _err_on_write_reserved(err_on_write_reserved) {}

    Err access(Vbus::Access access, const VcpuCtx*, uint64& value) override {
        if (access == Vbus::WRITE && !_writable)
            return Err::ACCESS_ERR;

        if (access == Vbus::WRITE) {
            // If writing 1 to a reserved value is not allowed, error out
            if (_err_on_write_reserved and ((value & ~_write_mask) != 0))
                return Err::ACCESS_ERR;

            _value |= value & _write_mask;                    // Set the bits at 1
            _value &= ~(_write_mask & (value ^ _write_mask)); // Set the bits at 0
        } else {
            value = _value;
        }

        return Err::OK;
    }

    void reset(const VcpuCtx*) override { _value = _reset_value; }
};

class Msr::BusStats {
public:
    explicit BusStats(MapKV<mword, RegisterBase>* devs) : _devices(devs) {}

    uint64 total_access{0ull};
    Tsc last_seen{0ull};
    const RegisterBase* last_access{nullptr};

    MapKV<mword, RegisterBase>::iterator begin() const { return _devices->begin(); }
    MapKV<mword, RegisterBase>::iterator end() const { return _devices->end(); }

private:
    MapKV<mword, RegisterBase>* const _devices;
};

/*
 * This is the bus that will handle all reads and writes
 * to system registers.
 */
class Msr::BaseBus {
public:
    BaseBus() : _msrs_stats(&_devices) {}

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

    /*! \brief Debug only: control the msr stats collection
     *  \param enabled collect msr stats?
     */
    void set_stats(bool enabled) { _stats_enabled = enabled; }

    RegisterBase* get_register_with_id(Msr::Id id) const { return reinterpret_cast<RegisterBase*>(get_device_at(id.id())); }

    const BusStats* get_stats_ptr() { return &_msrs_stats; }

private:
    static void reset_register_cb(Msr::RegisterBase*, const VcpuCtx*);

    void log_trace_info(const RegisterBase* reg, Vbus::Access access, uint64 val);

    MapKV<mword, RegisterBase> _devices;

    bool _trace{false};
    bool _fold{true};

    const RegisterBase* _last_access{nullptr};
    size_t _num_accesses{0};

    BusStats _msrs_stats;
    bool _stats_enabled{false};

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
