/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <cstring>
#include <fcntl.h>
#include <thread>

#include <model/aa64_timer.hpp>
#include <model/cpu.hpp>
#include <model/gic.hpp>
#include <model/psci.hpp>
#include <model/simple_as.hpp>
#include <msr/msr.hpp>
#include <pl011/pl011.hpp>
#include <platform/context.hpp>
#include <platform/log.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

enum Debug::Level Debug::current_level = Debug::NONE;
bool Stats::requested = false;

static Semaphore wait_sm;

class Dummy_vcpu : public Model::Cpu {
public:
    Dummy_vcpu(Model::GicD &gic) : Model::Cpu(&gic, 0, 0) {}

    virtual void recall(bool, RecallReason) override {
        DEBUG("VCPU recalled - an interrupt is waiting.");

        wait_sm.release();
    }
    virtual Errno run() override { return ENONE; }
};

int
main() {
    Platform_ctx ctx;
    Vbus::Bus vbus;
    Model::GicD gicd(Model::GIC_V2, 1);
    Msr::Bus msr_bus;

    bool ok = gicd.init();
    ASSERT(ok);

    ok = Model::Cpu::init(1);
    ASSERT(ok);

    Dummy_vcpu vcpu(gicd);
    ok = vcpu.setup(&ctx);
    ASSERT(ok);

    Model::Pl011 pl011(gicd, 0x42);
    Model::AA64Timer ptimer(gicd, 0, 0x12);

    ok = pl011.init(&ctx);
    ASSERT(ok);
    ok = ptimer.init_irq(0, 0x12, false, true);
    ASSERT(ok);

    Msr::Bus::PlatformInfo info;
    ok = msr_bus.setup_arch_msr(info, vbus, gicd);
    ASSERT(ok);
    ok = msr_bus.setup_aarch64_physical_timer(ptimer);
    ASSERT(ok);

    INFO("== Virtual Bus Testing/Demo app ==");
    INFO("Adding devices to the virtual bus");

    ok = ptimer.init_timer_loop(&ctx);

    std::thread timer_thread(Model::Timer::timer_loop, &ctx, &ptimer);
    timer_thread.detach();

    ptimer.wait_for_loop_start();

    ok = vbus.register_device(&pl011, 0x42000, 0x1000);
    ASSERT(ok == true);
    ok = vbus.register_device(&gicd, 0x43000, 0x1000);
    ASSERT(ok == true);

    off_t file_size = 4096;
    char fname[32];
    strncpy(fname, "/tmp/bhv-XXXXXX", 32);
    int fd = mkstemp(fname);
    unlink(fname);
    int rc = fallocate(fd, 0, 0, file_size);
    ASSERT(rc == 0);

    Model::SimpleAS as(Platform::Mem::MemDescr(fd), false);
    GPA gpa(0x10000000);
    ok = as.construct(gpa, size_t(file_size), false);
    ASSERT(ok == true);
    ok = vbus.register_device(&as, gpa.get_value(), mword(file_size));
    ASSERT(ok == true);

    vbus.iter_devices<const VcpuCtx>(Model::SimpleAS::flush_callback, nullptr);

    RegAccessor regs(ctx, 0);
    VcpuCtx vctx{nullptr, &regs, 0};
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

    uint64 res;
    Firmware::Psci::smc_call_service(vctx, regs, vbus, 0x84000003u, res);

    INFO("Done");

    close(fd);
    return 0;
}
