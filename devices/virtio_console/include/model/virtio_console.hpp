/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/virtio.hpp>
#include <platform/semaphore.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Virtio_console;
    struct Virtio_console_config;
    class Irq_controller;
}

struct Model::Virtio_console_config {
    uint16 cols;
    uint16 rows;
    uint32 num_ports;
    uint32 emerg_wr;
};

class Model::Virtio_console : public Vbus::Device, private Virtio::Device {
private:
    enum { RX = 0, TX = 1 };
    Virtio::Ram const _ram;
    Model::Virtio_console_config config __attribute__((aligned(8)));

    Semaphore *_sem;
    Virtio::Callback *_callback{nullptr};
    Virtio::Descriptor *_tx_desc{nullptr};
    bool _driver_initialized{false};
    Platform::Signal _sig_notify_empty_space;

    bool mmio_write(Vcpu_id const, uint64 const, uint8 const, uint64 const);
    bool mmio_read(Vcpu_id const, uint64 const, uint8 const, uint64 &) const;

    void _notify(uint32) override;
    void _driver_ok() override;

public:
    Virtio_console(Irq_controller &irq_ctlr, uint64 const guest_base, uint64 const host_base,
                   uint64 const size, uint16 const irq, uint16 const queue_entries, Semaphore *sem)
        : Vbus::Device("virtio console"), Virtio::Device(3, _ram, irq_ctlr, &config, sizeof(config),
                                                         irq, queue_entries),
          _ram(guest_base, size, host_base), _sem(sem) {}

    bool init(const Platform_ctx *ctx) { return _sig_notify_empty_space.init(ctx); }
    void register_callback(Virtio::Callback &callback) { _callback = &callback; }

    void release_buffer();

    bool to_guest(char *buff, uint32 size);
    char *from_guest(uint32 &size);
    void wait_for_available_buffer() { _sig_notify_empty_space.wait(); }

    virtual void reset(const Vcpu_ctx *) override {
        _sig_notify_empty_space.sig();
        _reset();
    }

    virtual Vbus::Err access(Vbus::Access, const Vcpu_ctx *, Vbus::Space, mword, uint8,
                             uint64 &) override;

    Virtio::Queue_data const &queue_data_rx() const { return _data[RX]; }
    Virtio::Queue_data const &queue_data_tx() const { return _data[TX]; }
};
