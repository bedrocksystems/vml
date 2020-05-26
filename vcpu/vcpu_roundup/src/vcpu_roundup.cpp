/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <arch/barrier.hpp>
#include <model/cpu.hpp>
#include <platform/atomic.hpp>
#include <platform/context.hpp>
#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/semaphore.hpp>
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
class Global_roundup_info {
public:
    uint16 num_vcpus;                 /*!< Number of VCPU(s) for this virtual machine */
    atomic<uint16> vcpus_off;         /*!< VCPU(s) started but offline (non-started by the guest) */
    atomic<uint16> vcpus_progressing; /*!< VCPU(s) currently making progress (emulation or not) */

    /*! \brief Atomic entry into the roundup logic
     *
     * Callers for roundup will compete in that function and only one will succeed
     * in starting a roundup. Others will wait here. Note that 'losers' will be marked
     * as 'done progressing' because they are now waiting (if called from a VCPU).
     *
     * \param from_vcpu is this called from the context of a VCPU thread?
     */
    void begin_roundup(bool from_vcpu = false) {
        bool in_recall = _in_roundup;

        do {
            in_recall = _in_roundup;

            if (__UNLIKELY__(in_recall)) {
                if (from_vcpu)
                    Vcpu::Roundup::vcpu_notify_done_progessing();
                wait_for_next_round();
            }
        } while (!_in_roundup.cas(in_recall, true));
    }

    /*! \brief Finish the round up, the next one can begin on exit of this function.
     *
     * One thing to note in this function: 'vcpus_progressing' is reset to 'num_vcpus'.
     * As a default, we assume everybody makes progress.
     */
    void end_roundup() {
        ASSERT(_in_roundup == true);
        _in_roundup = false;
        vcpus_progressing = num_vcpus;
        Barrier::rw_before_rw();
        signal_next_round();
    }

    /*! \brief Resource acquisition for this class
     */
    Errno init(const Platform_ctx* ctx, uint16 nvcpus) {
        num_vcpus = nvcpus;
        vcpus_off = nvcpus;
        vcpus_progressing = num_vcpus;

        if (!_sm_emulating.init(ctx) || !_sm_waiter.init(ctx) || !_sm_off.init(ctx))
            return ENOMEM;

        return ENONE;
    }

    void wait_for_next_round() { _sm_waiter.acquire(); }
    void signal_next_round() { _sm_waiter.release(); }

    void wait_for_emulation_end() { _sm_emulating.acquire(); }
    void signal_emulation_end() { _sm_emulating.release(); }

    void wait_for_all_vcpus_off() { _sm_off.acquire(); }
    void signal_all_vcpus_off() { _sm_off.release(); }

private:
    Semaphore _sm_waiter;
    Semaphore _sm_emulating;
    Semaphore _sm_off;
    atomic<bool> _in_roundup{false}; /*!< Are we already handling a roundup? */
};

static Global_roundup_info roundup_info;

Errno
Vcpu::Roundup::init(const Platform_ctx* ctx, uint16 num_vcpus) {
    return roundup_info.init(ctx, num_vcpus);
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
    Model::Cpu::recall_all();

    if (from_vcpu)
        Vcpu::Roundup::vcpu_notify_done_progessing();

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
    Model::Cpu::resume_all();
    roundup_info.end_roundup();
}

/*! \brief Signal that a VCPU has stopped progressing.
 *
 * The last VCPU to stop progressing will also signal the caller of 'roundup' and
 * will effectively unblock it.
 */
void
Vcpu::Roundup::vcpu_notify_done_progessing() {
    uint16 prev = roundup_info.vcpus_progressing.fetch_sub(1);
    if (prev == 1)
        roundup_info.signal_emulation_end();
}

void
Vcpu::Roundup::vcpu_notify_switched_off() {
    uint16 prev = roundup_info.vcpus_off.fetch_add(1);

    if (prev == roundup_info.num_vcpus - 1) {
        roundup_info.signal_all_vcpus_off();
    }
}

void
Vcpu::Roundup::vcpu_notify_switched_on() {
    roundup_info.vcpus_off.fetch_sub(1);
}

void
Vcpu::Roundup::wait_for_all_off() {
    roundup_info.wait_for_all_vcpus_off();
}
