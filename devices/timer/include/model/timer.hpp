/**
 * Copyright (C) 2019-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <model/irq_controller.hpp>
#include <model/vcpu_types.hpp>
#include <platform/atomic.hpp>
#include <platform/context.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>

namespace Model {
    class Timer;
    class PerCpuTimer;
    class GlobalTimer;
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
    IrqController *const _irq_ctlr;
    uint16 const _irq;

    void set_ready() { _ready_sig.sig(); }
    bool timer_wait_timeout(uint64 timeout_abs) { return _wait_timer.wait(timeout_abs); }
    void timer_wait() { _wait_timer.wait(); }
    void timer_wakeup() { _wait_timer.sig(); }

    void set_terminated() { _terminated_sig.sig(); }

    void clear_irq_status() {
        set_irq_status(false);
        deassert_irq();
    }

    virtual bool can_fire() const = 0;
    virtual bool is_irq_status_set() const = 0;
    virtual void set_irq_status(bool set) = 0;
    virtual uint64 get_timeout_abs() const = 0;
    virtual bool curr_timer_expired(uint64) const { return false; }

    uint64 curr_wait_timeout() const { return _curr_timeout; }

public:
    Timer(IrqController &irq_ctlr, uint16 const irq) : _irq_ctlr(&irq_ctlr), _irq(irq) {}

    virtual bool assert_irq() = 0;
    virtual void deassert_irq() = 0;

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
     *  \param arg The timer object that will be partially owned by the timer loop thread
     */
    static void timer_loop(const Platform_ctx *ctx, void *arg);

    void terminate();

    void wait_for_loop_terminated() { _terminated_sig.wait(); }

    void cleanup_timer_loop_resources(const Platform_ctx *ctx);
};

class Model::PerCpuTimer : public Model::Timer {
private:
    Vcpu_id const _vcpu;

public:
    PerCpuTimer(IrqController &irq_ctlr, Vcpu_id const vcpu_id, uint16 const irq) : Model::Timer(irq_ctlr, irq), _vcpu(vcpu_id) {}

    bool init_irq(Vcpu_id const vcpu_id, uint16 const pirq, bool hw, bool edge = true) {
        return _irq_ctlr->config_irq(vcpu_id, _irq, hw, pirq, edge);
    }
    bool assert_irq() override { return _irq_ctlr->assert_ppi(_vcpu, _irq); }
    void deassert_irq() override { _irq_ctlr->deassert_line_ppi(_vcpu, _irq); }
};

class Model::GlobalTimer : public Model::Timer {
private:
public:
    GlobalTimer(IrqController &irq_ctlr, uint16 const irq) : Model::Timer(irq_ctlr, irq) {}

    bool init_irq(uint16 const pirq, bool hw, bool edge = true) { return _irq_ctlr->config_spi(_irq, hw, pirq, edge); }
    bool assert_irq() override { return _irq_ctlr->assert_global_line(_irq); }
    void deassert_irq() override { _irq_ctlr->deassert_global_line(_irq); }
};
