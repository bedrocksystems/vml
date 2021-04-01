/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <thread>

#include <model/cpu.hpp>
#include <model/gic.hpp>
#include <model/physical_timer.hpp>
#include <model/psci.hpp>
#include <model/simple_as.hpp>
#include <msr/msr.hpp>
#include <pl011/pl011.hpp>
#include <platform/context.hpp>
#include <platform/log.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

enum Debug::Level Debug::current_level = Debug::None;

static Semaphore wait_sm;

class Dummy_vcpu : public Model::Cpu {
public:
    Dummy_vcpu(Model::Gic_d &gic) : Model::Cpu(&gic, 0, 0) {}

    virtual bool recall(bool) override {
        DEBUG("VCPU recalled - an interrupt is waiting.");

        wait_sm.release();
        return true;
    }
    virtual Errno run() override { return ENONE; }
    virtual void ctrl_tvm(bool, Request::Requestor, const Reg_selection) override {}
    virtual void ctrl_single_step(bool, Request::Requestor) override {}
};

int
main() {
    Platform_ctx ctx;
    Vbus::Bus vbus;
    Model::Gic_d gicd(Model::GIC_V2, 1);
    Model::Simple_as as(false);
    Msr::Bus msr_bus;

    bool ok = gicd.init();
    ASSERT(ok);

    ok = Model::Cpu::init(1);
    ASSERT(ok);

    Dummy_vcpu vcpu(gicd);
    ok = vcpu.setup(&ctx);
    ASSERT(ok);

    Model::Pl011 pl011(gicd, 0x42);
    Model::Physical_timer ptimer(gicd, 0, 0x12);

    ok = pl011.init(&ctx);
    ASSERT(ok);
    ok = ptimer.init_irq(0, 0x12, false, true);
    ASSERT(ok);

    Msr::Bus::Platform_info info;
    ok = msr_bus.setup_arch_msr(info, vbus, gicd);
    ASSERT(ok);
    ok = msr_bus.setup_aarch64_physical_timer(ptimer);
    ASSERT(ok);

    INFO("== Virtual Bus Testing/Demo app ==");
    INFO("Adding devices to the virtual bus");

    ok = ptimer.init(&ctx);

    std::thread timer_thread(Model::Physical_timer::timer_loop, &ctx, &ptimer);
    timer_thread.detach();

    ptimer.wait_for_loop_start();

    ok = vbus.register_device(&pl011, 0x42000, 0x1000);
    ASSERT(ok == true);
    ok = vbus.register_device(&gicd, 0x43000, 0x1000);
    ASSERT(ok == true);

    char *guest_as = new char[4096];
    as.set_guest_as(reinterpret_cast<mword>(guest_as), 4096);
    ok = vbus.register_device(&as, reinterpret_cast<mword>(guest_as), 4096);
    ASSERT(ok == true);

    vbus.iter_devices(Model::Simple_as::flush_callback, nullptr);

    Reg_accessor regs(ctx, 0);
    Vcpu_ctx vctx{nullptr, &regs, 0};
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

    return 0;
}
