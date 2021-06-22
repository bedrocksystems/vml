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
    class VirtioMMIO_console;
    struct Virtio_console_config;
    class Irq_controller;
}

struct Model::Virtio_console_config {
    uint16 cols;
    uint16 rows;
    uint32 num_ports;
    uint32 emerg_wr;
};

class Model::VirtioMMIO_console : public Virtio::Virtio_console, public Virtio::Device {
private:
    enum { RX = 0, TX = 1 };
    Model::Virtio_console_config _config __attribute__((aligned(8)));

    Platform::Signal *_sig_notify_event;
    Virtio::Descriptor *_tx_desc{nullptr};
    bool _driver_initialized{false};
    Platform::Signal _sig_notify_empty_space;

    void notify(uint32) override;
    void driver_ok() override;

public:
    VirtioMMIO_console(Irq_controller &irq_ctlr, const Vbus::Bus &bus, uint16 const irq,
                       uint16 const queue_entries, Platform::Signal *sig)
        : Virtio::Device("virtio console", 3, bus, irq_ctlr, &_config, sizeof(_config), irq,
                         queue_entries),
          _sig_notify_event(sig) {}

    bool init(const Platform_ctx *ctx) { return _sig_notify_empty_space.init(ctx); }

    virtual void release_buffer() override;

    virtual bool to_guest(const char *buff, uint32 size) override;
    virtual const char *from_guest(uint32 &size) override;
    virtual void wait_for_available_buffer() override { _sig_notify_empty_space.wait(); }

    virtual void reset(const VcpuCtx *) override {
        _sig_notify_empty_space.sig();
        reset_virtio();
    }

    Virtio::QueueData const &queue_data_rx() const { return queue_data(RX); }
    Virtio::QueueData const &queue_data_tx() const { return queue_data(TX); }
};
