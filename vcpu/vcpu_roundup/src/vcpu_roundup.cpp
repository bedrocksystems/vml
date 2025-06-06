/*
 * Copyright (C) 2020-2025 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <model/cpu.hpp>
#include <model/vcpu_types.hpp>
#include <platform/atomic.hpp>
#include <platform/compiler.hpp>
#include <platform/context.hpp>
#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/mutex.hpp>
#include <platform/semaphore.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <vcpu/vcpu_roundup.hpp>

/*! \file
 * \brief This file contains the logic of the roundup code.
 *
 * Let's describe the goal of the code below at a high level: we want
 * to gather all the VCPU(s) of the running VM. This is useful in several
 * cases like: reboot, introspection, etc.. Several approaches can be taken
 * to achieve this. The approach that we decide to take is: roundup ensures
 * that no VCPU will make progress in the guest execution after the call
 * returns. The guest can make progress in two ways: via direct execution of
 * the code on the CPU or via emulation in the VMM. Both of those should be
 * stopped for a successful roundup. NOVA provides a "recall" function that
 * guarantees that the guest is not executing anymore upon completion of the
 * hypercall. We will leverage this along with some internal VCPU state inside
 * the VMM to make sure that the guest is not making progress.
 */

/*! \brief Global (static and unique) object containing the state of the current roundup
 */
class GlobalRoundupInfo {
public:
    uint16 num_vcpus{0};              /*!< Number of VCPU(s) for this virtual machine */
    atomic<uint16> vcpus_progressing; /*!< VCPU(s) currently making progress (emulation or not) */

    void yield() {
        _vcpu_waiters++;
        vcpu_notify_done_progressing();
    }

    void unyield() {
        auto waiters_old = _vcpu_waiters--;
        ASSERT(waiters_old >= 1);
    }

    /*! \brief Atomic entry into the roundup logic
     *
     * Callers for roundup will compete in that function and only one will succeed
     * in starting a roundup. Others will wait here. Note that 'losers' will be marked
     * as 'done progressing' because they are now waiting (if called from a VCPU).
     *
     * \param from_vcpu is this called from the context of a VCPU thread?
     *
     * Note: we disable thread safety analysis for now because enabling it would require
     * annotating the roundup/resume functions and would leak private details to the caller.
     * We can refactor that code later if needed.
     */
    void begin_roundup(bool from_vcpu) NO_THREAD_SAFETY_ANALYSIS {
        if (from_vcpu)
            yield();
        bool ok = _waiter_mutex.enter();
        ASSERT(ok);
    }

    /*! \brief Finish the round up, the next one can begin on exit of this function.
     *
     * One thing to note in this function: 'vcpus_progressing' is reset to 'num_vcpus - _vcpu_waiters'.
     * As a default, we assume everybody makes progress, except CPUs that are waiting on roundup.
     */
    void end_roundup(bool from_vcpu) {
        if (from_vcpu)
            unyield();
        end_roundup_core();
    }

    void signal_next_waiter() NO_THREAD_SAFETY_ANALYSIS {
        bool ok = _waiter_mutex.exit();
        ASSERT(ok);
    }

    /*! \brief Resource acquisition for this class
     */
    Errno init(const Platform_ctx* ctx, uint16 nvcpus) {
        num_vcpus = nvcpus;
        vcpus_progressing = num_vcpus;
        _vcpu_waiters = 0;

        if (!_sig_emulating.init(ctx) || !_waiter_mutex.init(ctx))
            return Errno::NOMEM;

        return Errno::NONE;
    }

    Errno cleanup(const Platform_ctx* ctx) {
        _sig_emulating.destroy();
        return _waiter_mutex.destroy(ctx);
    }

    void wait_for_emulation_end() { _sig_emulating.wait(); }
    void signal_emulation_end() { _sig_emulating.sig(); }

    /*! \brief Signal that a VCPU has stopped progressing.
     *
     * The last VCPU to stop progressing will also signal the caller of 'roundup' and
     * will effectively unblock it.
     */
    void vcpu_notify_done_progressing() {
        auto progressing = vcpus_progressing.fetch_sub(1);
        ASSERT(progressing != 0);
        if (progressing == 1)
            signal_emulation_end();
    }

private:
    inline void end_roundup_core() {
        uint16 num_waiters = _vcpu_waiters;
        ASSERT(num_waiters <= num_vcpus);
        vcpus_progressing = num_vcpus - num_waiters;
    }

    Platform::Mutex _waiter_mutex;
    Platform::Signal _sig_emulating;
    atomic<uint16> _vcpu_waiters;
};

class VcpuInitializedInfo {
public:
    Errno init(const Platform_ctx* ctx) {
        vcpus_startup_done = 0;
        if (!_sm_all_initialized.init(ctx))
            return Errno::NOMEM;

        return Errno::NONE;
    }

    Errno cleanup(const Platform_ctx* ctx) { return _sm_all_initialized.destroy(ctx); }

    void wait_for_all_vcpus_initialized() { _sm_all_initialized.wait(); }
    void signal_all_vcpus_initialized() { _sm_all_initialized.sig(); }

    atomic<uint16> vcpus_startup_done; /*!< VCPU(s) that are not yet fully initialized */

private:
    Platform::Signal _sm_all_initialized;
};

struct ParallelRoundupInfo {
    Semaphore count_sem;
    Semaphore resume_waiter_sem;
    atomic<uint16> count;
    uint16 num_waiters{0};
};

static GlobalRoundupInfo roundup_info;
static ParallelRoundupInfo parallel_info;
static VcpuInitializedInfo initialized_info;

Errno
Vcpu::Roundup::init(const Platform_ctx* ctx, uint16 num_vcpus) {
    parallel_info.num_waiters = 0;
    parallel_info.count = 0;
    Errno err = roundup_info.init(ctx, num_vcpus);
    if (err != Errno::NONE)
        return err;
    if (!parallel_info.count_sem.init(ctx) || !parallel_info.resume_waiter_sem.init(ctx))
        return Errno::NOMEM;

    return initialized_info.init(ctx);
}

Errno
Vcpu::Roundup::cleanup(const Platform_ctx* ctx) {
    if (Errno::NONE != parallel_info.count_sem.destroy(ctx) || Errno::NONE != parallel_info.resume_waiter_sem.destroy(ctx))
        return Errno::BADR;

    Errno err = initialized_info.cleanup(ctx);
    if (err != Errno::NONE)
        return err;

    return roundup_info.cleanup(ctx);
}

void
Vcpu::Roundup::vcpu_notify_done_progressing() {
    roundup_info.vcpu_notify_done_progressing();
}

/*! \brief Internal roundup function
 *
 * Begins the round up logic. We recall all the VCPU(s) and wait from
 * them to be done making progress (emulation or in direct execution).
 *
 * \param from_vcpu Are we called from the context of a VCPU?
 */
static inline void
do_roundup(bool from_vcpu) {
    roundup_info.begin_roundup(from_vcpu);
    Model::Cpu::roundup_all();

    while (roundup_info.vcpus_progressing != 0)
        roundup_info.wait_for_emulation_end();
}

/*! \brief Main roundup function. The caller is assumed to not be a VCPU thread.
 */
void
Vcpu::Roundup::roundup() {
    do_roundup(false);
}

/*! \brief Round up logic if the caller is in the context of a VCPU thread
 */
void
Vcpu::Roundup::roundup_from_vcpu(Vcpu_id) {
    do_roundup(true);
}

/*! \brief Allow the VM to make progress again. This signals the end of the roundup.
 */
void
Vcpu::Roundup::resume() {
    roundup_info.end_roundup(false);
    Model::Cpu::resume_all();
    roundup_info.signal_next_waiter();
}

/*! \brief Allow the VM to make progress again. This signals the end of the roundup from a VCPU.
 */
void
Vcpu::Roundup::resume_from_vcpu(Vcpu_id) {
    roundup_info.end_roundup(true);
    Model::Cpu::resume_all();
    roundup_info.signal_next_waiter();
}

void
Vcpu::Roundup::vcpu_notify_initialized() {
    uint16 total = initialized_info.vcpus_startup_done.add_fetch(1);

    if (total == roundup_info.num_vcpus) {
        initialized_info.signal_all_vcpus_initialized();
    }
}

void
Vcpu::Roundup::wait_for_all_off() {
    initialized_info.wait_for_all_vcpus_initialized();
}

void
Vcpu::Roundup::roundup_parallel(Vcpu_id id) {
    uint16 count = parallel_info.count.fetch_add(1);

    if (count == 0) {
        roundup_from_vcpu(id);
        uint16 parallel_callers = parallel_info.count - 1; // This is stable now
        parallel_info.num_waiters = parallel_callers;
        while (parallel_callers > 0) {
            parallel_info.count_sem.release();
            parallel_callers--;
        }
    } else {
        roundup_info.yield();   // Signal that we are waiting and not progressing anymore
        parallel_info.count_sem.acquire();
        roundup_info.unyield(); // Progress resumed, we are not waiting anymore
    }
}

void
Vcpu::Roundup::resume_parallel(Vcpu_id id) {
    ASSERT(parallel_info.count != 0);
    uint16 cur_count = parallel_info.count.sub_fetch(1);

    if (cur_count == 0) {
        Vcpu::Roundup::resume_from_vcpu(id);
        while (parallel_info.num_waiters > 0) {
            parallel_info.resume_waiter_sem.release();
            parallel_info.num_waiters--;
        }
    } else {
        parallel_info.resume_waiter_sem.acquire();
    }
}
