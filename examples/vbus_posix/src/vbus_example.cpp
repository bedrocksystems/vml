/**
 * Copyright (C) 2020-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <cstring>
#include <fcntl.h>
#include <model/aa64_timer.hpp>
#include <model/cpu.hpp>
#include <model/gic.hpp>
#include <model/simple_as.hpp>
#include <pl011/pl011.hpp>
#include <platform/context.hpp>
#include <platform/log.hpp>
#include <platform/memory.hpp>
#include <platform/reg_accessor.hpp>
#include <platform/semaphore.hpp>
#include <platform/types.hpp>
#include <thread>
#include <vbus/vbus.hpp>

static Semaphore wait_sm;

class Dummy_vcpu : public Model::Cpu {
public:
    Dummy_vcpu(Model::GicD& gic) : Model::Cpu(&gic, 0, 0) {}

    virtual void recall(bool, RecallReason) override {
        DEBUG("VCPU recalled - an interrupt is waiting.");

        wait_sm.release();
    }
};

int
main() {
    Platform_ctx ctx;
    Vbus::Bus vbus;
    Model::GicD gicd(Model::GIC_V2, 1, nullptr);

    bool ok = gicd.init();
    ASSERT(ok);

    ok = Model::Cpu::init(1);
    ASSERT(ok);

    Dummy_vcpu vcpu(gicd);
    ok = vcpu.setup(&ctx);
    ASSERT(ok);
    vcpu.switch_state_to_on();

    Model::Pl011 pl011(gicd, 0x42);
    Model::AA64Timer ptimer(gicd, 0, 0x12);

    ok = pl011.init(&ctx);
    ASSERT(ok);
    ok = ptimer.init_irq(0, 0x12, false, true);
    ASSERT(ok);

    INFO("== Virtual Bus Testing/Demo app ==");
    INFO("Adding devices to the virtual bus");

    ok = ptimer.init_timer_loop(&ctx);

    std::thread timer_thread(Model::Timer::timer_loop, &ctx, &ptimer);

    ptimer.wait_for_loop_start();

    ok = vbus.register_device(&pl011, 0x42000, 0x1000);
    ASSERT(ok == true);
    ok = vbus.register_device(&gicd, 0x43000, 0x1000);
    ASSERT(ok == true);

    off_t file_size = 4096;
    static const char* TMP_FILE = "vml-vbus-example";

    shm_unlink(TMP_FILE); // In case there was a file left behind
    int fd = shm_open(TMP_FILE, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("shm_open");
        exit(1);
    }

    int rc = ftruncate(fd, file_size);
    if (rc == -1) {
        perror("ftruncate");
        exit(1);
    }

    GPA gpa(0x10000000);
    Model::SimpleAS as(Range<mword>{gpa.get_value(), size_t(file_size)}, Platform::Mem::MemDescr(fd), Platform::Mem::Cred{});
    ok = vbus.register_device(&as, gpa.get_value(), mword(file_size));
    ASSERT(ok == true);

    RegAccessor regs(ctx, 0);
    VcpuCtx vctx{&regs, 0};
    uint64 val;

    INFO("Accessing the GIC model");
    Vbus::Err err = vbus.access(Vbus::READ, vctx, 0x43000, 4, val);
    ASSERT(err == Vbus::OK);

    std::chrono::steady_clock::time_point n = std::chrono::steady_clock::now();
    n += std::chrono::seconds(2);

    ptimer.set_cval(static_cast<unsigned long long>(n.time_since_epoch().count()));
    ptimer.set_ctl(0b1); // Enable the physical timer

    INFO("Waiting for the timer interrupt (2s wait)");
    wait_sm.acquire();

    ptimer.terminate();
    timer_thread.join();

    INFO("Done");
    rc = shm_unlink(TMP_FILE);
    if (rc != 0) {
        perror("shm_unlink");
        exit(1);
    }

    return 0;
}
