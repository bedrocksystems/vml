/*
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/timer.hpp>

namespace Model {
    class AA64Timer;
}

class Model::AA64Timer : public Model::Timer {
private:
    enum : uint8 { ENABLED_BIT = 0x1, MASKED_BIT = 0x2, STATUS_BIT = 0x4 };

    class CntvCtl {
    public:
        explicit CntvCtl(uint8 val) : _value(val) {}

        constexpr bool enabled() const { return (_value & ENABLED_BIT) != 0; }
        constexpr bool masked() const { return (_value & MASKED_BIT) != 0; }
        constexpr bool status() const { return (_value & STATUS_BIT) != 0; }
        constexpr bool can_fire() const { return enabled() && !masked(); }
        void set_status(bool set) {
            if (set)
                _value |= STATUS_BIT;
            else
                _value &= static_cast<uint8>(~STATUS_BIT);
        }

        constexpr uint8 get() const { return _value; }
        void set(uint8 val) { _value = val; }

    private:
        uint8 _value;
    };

public:
    /*! \brief Construct a physical timer
     *  \pre Requires giving up a fractional ownership of the GIC to this class. The caller
     *  will have to provide a valid VCPU id and the physical timer IRQ configuration. Typically,
     *  the physical timer configuration is determined by the config in the guest FDT.
     *  \post Full ownership on a valid (but not initialized) timer object
     *  \param gic The GIC that will receive interrupts from the timer
     *  \param cpu The id of the VCPU that owns this physical timer
     *  \param irq The IRQ number associated with the timer (should be a PPI)
     */
    AA64Timer(Irq_controller &irq_ctlr, Vcpu_id const cpu, uint16 const irq)
        : Timer(irq_ctlr, cpu, irq), _cntv_ctl(0), _cval(0) {}

    /*! \brief Set the compare value of the timer
     *  \pre Fractional ownership of an initialized timer object.
     *  \post Ownership unchanged and the internal cval value was set to the given parameter.
     *  The timer loop will be awaken as a result of this call to evaluate if the timer should
     *  be reconfigured.
     *  Note that due to the nature of this device: only the VCPU associated with this timer
     *  can access this. In other words, this cannot be called in parallel.
     *  \param cval The compare value to set (in system ticks)
     */
    void set_cval(uint64 cval) {
        _cval = cval;
        timer_wakeup();
    }

    /*! \brief Get the compare value of the timer
     *  \pre Fractional ownership of an initialized timer object.
     *  \post Ownership unchanged and the return value is set to the internal cval
     *  \return the current cval value
     */
    uint64 get_cval() const { return _cval; }

    /*! \brief Set the control value of the timer
     *  \pre Fractional ownership of an initialized timer object.
     *  \post Ownership unchanged and the internal control value was set to the given parameter.
     *  The timer loop might be awaken as a result of this call to evaluate if the timer should
     *  be reconfigured. Note that due to the nature of this device: only the VCPU associated with
     * this timer can access this. In other words, this cannot be called in parallel.
     *  \param cval The compare value to set (in system ticks)
     */
    void set_ctl(uint8 ctl) {
        _cntv_ctl.set(ctl);
        if (_cntv_ctl.can_fire())
            timer_wakeup();
    }

    /*! \brief Get the control value of the timer
     *  \pre Fractional ownership of an initialized timer object.
     *  \post Ownership unchanged and the return value is set to the internal control value
     *  \return the current control value
     */
    uint8 get_ctl() const { return _cntv_ctl.get(); }

    bool will_timeout(uint64 const control) {
        CntvCtl ctl = CntvCtl(uint8(control));
        return ctl.can_fire();
    }

private:
    virtual bool can_fire() const override { return _cntv_ctl.can_fire(); }
    virtual bool is_irq_status_set() const override { return _cntv_ctl.status(); };
    virtual void set_irq_status(bool set) override { _cntv_ctl.set_status(set); }
    virtual uint64 get_timeout_abs() const override { return get_cval(); }

    CntvCtl _cntv_ctl;
    uint64 _cval;
};
