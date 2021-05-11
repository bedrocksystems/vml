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
#include <model/virtio_console.hpp>
#include <platform/context.hpp>
#include <platform/log.hpp>
#include <platform/reg_accessor.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

static const constexpr uint32 VIRTIO_RAM_SIZE = 0x10000;
static uint8 vRAM[VIRTIO_RAM_SIZE];
static const constexpr uint64 VIRTIO_BASE = 0x44000;
static const uint64 VIRTIO_GUEST_BASE = reinterpret_cast<uint64>(vRAM);

static const uint64 Q0_DESC = VIRTIO_GUEST_BASE;
static const uint64 Q0_DRIVER = VIRTIO_GUEST_BASE + 0x1000;
static const uint64 Q0_DEVICE = VIRTIO_GUEST_BASE + 0x2000;

static const uint64 Q1_DESC = VIRTIO_GUEST_BASE + 0x3000;
static const uint64 Q1_DRIVER = VIRTIO_GUEST_BASE + 0x4000;
static const uint64 Q1_DEVICE = VIRTIO_GUEST_BASE + 0x5000;

enum Debug::Level Debug::current_level = Debug::NONE;

static Semaphore wait_sm;

class Dummy_vcpu : public Model::Cpu {
public:
    Dummy_vcpu(Model::GicD &gic) : Model::Cpu(&gic, 0, 0) {}

    virtual bool recall(bool) override { return true; }
    virtual Errno run() override { return ENONE; }
    virtual void ctrl_tvm(bool, Request::Requestor, const Reg_selection) override {}
    virtual void ctrl_single_step(bool, Request::Requestor) override {}
};

class Dummy_Virtio_Interface : public Virtio::Callback {
public:
    Dummy_Virtio_Interface() {}

    void driver_ok() override {
        DEBUG("Driver OK callback from model");
        wait_sm.release();
    }
};

void
init_virtio_console(Vbus::Bus &vbus, VcpuCtx &vctx) {
    uint64 val;
    Vbus::Err err;

    // Reset.
    val = 0x0;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x70, 4, val);
    ASSERT(err == Vbus::OK);

    // Select queue 0.
    val = 0;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x30, 4, val);
    ASSERT(err == Vbus::OK);

    // Num queue.
    val = 10;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x38, 4, val);
    ASSERT(err == Vbus::OK);

    // Desc low
    val = Q0_DESC;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x80, 4, val);
    ASSERT(err == Vbus::OK);

    // Driver low
    val = Q0_DRIVER;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x90, 4, val);
    ASSERT(err == Vbus::OK);

    // Device low
    val = Q0_DEVICE;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0xA0, 4, val);
    ASSERT(err == Vbus::OK);

    // Queue ready
    val = 1;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x44, 4, val);
    ASSERT(err == Vbus::OK);

    // Select queue 1.
    val = 1;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x30, 4, val);
    ASSERT(err == Vbus::OK);

    // Num queue.
    val = 10;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x38, 4, val);
    ASSERT(err == Vbus::OK);

    // Desc low
    val = Q1_DESC;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x80, 4, val);
    ASSERT(err == Vbus::OK);

    // Driver low
    val = Q1_DRIVER;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x90, 4, val);
    ASSERT(err == Vbus::OK);

    // Device low
    val = Q1_DEVICE;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0xA0, 4, val);
    ASSERT(err == Vbus::OK);

    // Queue ready
    val = 1;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x44, 4, val);
    ASSERT(err == Vbus::OK);

    // Driver OK.
    val = 0x4;
    err = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x70, 4, val);
    ASSERT(err == Vbus::OK);
}

int
main() {
    Platform_ctx ctx;
    Vbus::Bus vbus;
    Model::GicD gicd(Model::GIC_V2, 1);

    bool ok = gicd.init();
    ASSERT(ok);

    ok = Model::Cpu::init(1);
    ASSERT(ok);

    Dummy_vcpu vcpu(gicd);

    Semaphore sem;
    ok = sem.init(&ctx);
    ASSERT(ok);

    ok = vcpu.setup(&ctx);
    ASSERT(ok);

    Vbus::Bus bus;
    Model::SimpleAS sas(false);

    sas.set_guest_as(VIRTIO_GUEST_BASE, VIRTIO_RAM_SIZE);

    ok = bus.register_device(&sas, VIRTIO_GUEST_BASE, VIRTIO_RAM_SIZE);
    ASSERT(ok);

    Model::VirtioMMIO_console virtio_console(gicd, bus, 0x13, 10, &sem);

    Dummy_Virtio_Interface virtio_interface;
    virtio_console.register_callback(&virtio_interface);

    INFO("== Virtio Test application ==");
    INFO("Adding devices to the virtual bus");

    ok = vbus.register_device(&gicd, 0x43000, 0x1000);
    ASSERT(ok == true);

    ok = vbus.register_device(&virtio_console, VIRTIO_BASE, 0x1000);
    ASSERT(ok == true);

    RegAccessor regs(ctx, 0);
    VcpuCtx vctx{nullptr, &regs, 0};
    uint64 val;

    INFO("Accessing the Virtio console model");
    Vbus::Err err = vbus.access(Vbus::READ, vctx, VIRTIO_BASE, 4, val);
    ASSERT(err == Vbus::OK);

    INFO("Mocking virtio init on virtio console");
    init_virtio_console(vbus, vctx);

    wait_sm.acquire();
    INFO("Virtio device initialized");

    INFO("Testing virtio driver kick");
    val = 1;
    ok = vbus.access(Vbus::WRITE, vctx, VIRTIO_BASE + 0x50, 4, val);
    ASSERT(ok == Vbus::OK);

    sem.acquire();
    INFO("Virtio console received kick");

    return 0;
}
