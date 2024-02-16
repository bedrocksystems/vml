/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/irq_controller.hpp>
#include <model/vcpu_types.hpp>
#include <platform/errno.hpp>
#include <platform/signal.hpp>

namespace Model {
    class Timer;
};

class Model::Timer {
private:
    Platform::Signal _ready_sig;
    Platform::Signal _wait_timer;
    Platform::Signal _terminated_sig;

    atomic<bool> _terminate{false};
    uint64 _curr_timeout{0};

    void set_wait_timeout(uint64 timeout) { _curr_timeout = timeout; }

protected:
    Irq_controller *const _irq_ctlr;
    Vcpu_id const _vcpu;
    uint16 const _irq;

    void set_ready() { _ready_sig.sig(); }
    bool timer_wait_timeout(uint64 timeout_abs) { return _wait_timer.wait(timeout_abs); }
    void timer_wait() { return _wait_timer.wait(); }
    void timer_wakeup() { _wait_timer.sig(); }

    void set_terminated() { _terminated_sig.sig(); }

    void clear_irq_status() {
        set_irq_status(false);
        _irq_ctlr->deassert_line_ppi(_vcpu, _irq);
    }

    virtual bool can_fire() const = 0;
    virtual bool is_irq_status_set() const = 0;
    virtual void set_irq_status(bool set) = 0;
    virtual uint64 get_timeout_abs() const = 0;
    virtual bool curr_timer_expired(uint64) const { return false; };

    uint64 curr_wait_timeout() const { return _curr_timeout; }

public:
    Timer(Irq_controller &irq_ctlr, Vcpu_id const vcpu_id, uint16 const irq) : _irq_ctlr(&irq_ctlr), _vcpu(vcpu_id), _irq(irq) {}

    bool init_irq(Vcpu_id const vcpu_id, uint16 const pirq, bool hw, bool edge = true) {
        return _irq_ctlr->config_irq(vcpu_id, _irq, hw, pirq, edge);
    }

    bool assert_irq() { return _irq_ctlr->assert_ppi(_vcpu, _irq); }

    uint16 irq_num() const { return _irq; }

    /*! \brief Wait for the timer loop to start before returning
     *  \pre Partial ownership of an initialized timer object
     *  \post No change in ownership. Will only return once timer_loop has started.
     */
    void wait_for_loop_start() { _ready_sig.wait(); }

    /*! \brief Initialize the timer object - this function must called before any other call
     *  \pre Full ownership of an non-initialized timer object
     *  \post If the result is true, returns the full ownership of the timer object with its
     *  internal state as initialized. Otherwise, the ownership is unchanged and the timer stay
     *  uninitialized.
     *  \param ctx Platform specific data
     *  \return true on success, false otherwise
     */
    bool init_timer_loop(const Platform_ctx *ctx);

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
    static void timer_loop(const Platform_ctx *ctx, Model::Timer *timer);

    void terminate();

    void wait_for_loop_terminated() { _terminated_sig.wait(); }

    bool cleanup_timer_loop_resources(const Platform_ctx *ctx);
};
