/*
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

/*! \file Emulation logic for a Physical Timer on ARM
 */

#include <model/gic.hpp>
#include <model/timer.hpp>
#include <platform/log.hpp>
#include <platform/semaphore.hpp>

namespace Model {
    class Physical_timer;
}

/*! \brief CPU Private Physical timer emulation logic
 */
class Model::Physical_timer : public Model::Timer {
public:
    /*! \brief Construct a physical timer
     *  \pre Requires giving up a fractional ownership of the GIC to this class. The caller
     *  will have to provide a valid VCPU id and the physical timer IRQ configuration. Typically,
     *  the physical timer configuration is determined by the config in the guest FDT.
     *  \post Full ownership on a valid (but not initialized) timer object
     *  \param gic The GIC that will receive interrupts from the timer
     *  \param cpu The id of the VCPU that owns this physical timer
     *  \param irq The IRQ number associated with the timer (should be a PPI)
     *  \param edge Is the IRQ edge triggered? If not, it will be level-triggered
     */
    Physical_timer(Gic_d &gic, Vcpu_id const cpu, uint16 const irq, bool edge)
        : Timer(gic, cpu, irq, irq, false, edge), _cntv_ctl(0), _cval(0), _wait_timer(),
          _ready_sm() {}

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
        _wait_timer.release();
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
            _wait_timer.release();
    }

    /*! \brief Get the control value of the timer
     *  \pre Fractional ownership of an initialized timer object.
     *  \post Ownership unchanged and the return value is set to the internal control value
     *  \return the current control value
     */
    uint8 get_ctl() const { return _cntv_ctl.get(); }

    /*! \brief Initialize the timer object - this function must called before any other call
     *  \pre Full ownership of an non-initialized timer object
     *  \post If the result is true, returns the full ownership of the timer object with its
     *  internal state as initialized. Otherwise, the ownership is unchanged and the timer stay
     *  uninitialized.
     *  \param ctx Platform specific data
     *  \return true on success, false otherwise
     */
    bool init(const Platform_ctx *ctx);

    /*! \brief Internal timer loop that sends IRQs to the GIC
     *
     *  The caller is expected to call this function from a separate thread that it has
     *  previously created. The thread will then be considered as 'detached' and cannot
     *  join the main thread. This is because a physical timer is not meant to be stopped.
     *
     *  \pre Full ownership of an initialized timer object
     *  \post Fractional ownership of an initialized timer object (part of the ownership is
     *  kept by the thread).
     *  \param ctx Platform specific data
     *  \param timer The timer object that will be partially owned by the timer loop thread
     */
    [[noreturn]] static void timer_loop(const Platform_ctx *ctx, Model::Physical_timer *timer);

    /*! \brief Wait for the timer loop to start before returning
     *  \pre Partial ownership of an initialized timer object
     *  \post No change in ownership. Will only return once timer_loop has started.
     */
    void wait_for_loop_start() { _ready_sm.acquire(); }

private:
    bool can_fire() const { return _cntv_ctl.can_fire(); }
    void set_ready() { _ready_sm.release(); }
    Semaphore &get_timer_sm() { return _wait_timer; }

    bool is_istatus_set() const { return _cntv_ctl.status(); };
    void set_istatus() { _cntv_ctl.set_status(); }
    void clear_istatus() {
        _cntv_ctl.clear_status();
        _gic->deassert_line_ppi(_vcpu, _irq);
    }

    Cntv_ctl _cntv_ctl;
    uint64 _cval;
    Semaphore _wait_timer;
    Semaphore _ready_sm;
};