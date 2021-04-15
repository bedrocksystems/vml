/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/virtio.hpp>
#include <platform/semaphore.hpp>
#include <platform/string.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Virtio_sock;
    struct Virtio_sock_config;
    class Virtio_sock_callback;
    class Irq_contoller;
}

struct Model::Virtio_sock_config {
    uint64 guest_cid;
};

class Model::Virtio_sock_callback {
public:
    virtual void device_reset(const VcpuCtx *ctx) = 0;
};

class Model::Virtio_sock : public Vbus::Device, public Virtio::Device {

private:
    enum { RX = 0, TX = 1, EVENT = 2 };
    Virtio::Callback *_callback{nullptr};
    Model::Virtio_sock_callback *_virtio_sock_callback{nullptr};
    Virtio_sock_config config;
    Semaphore *_sem;
    bool _backend_connected{false};

    bool mmio_write(Vcpu_id const, uint64 const, uint8 const, uint64 const);
    bool mmio_read(Vcpu_id const, uint64 const, uint8 const, uint64 &) const;

    void _notify(uint32) override;
    void _driver_ok() override;

public:
    Virtio_sock(Irq_controller &irq_ctlr, const Vbus::Bus &bus, uint16 const irq,
                uint16 const queue_entries, uint64 cid, Semaphore *sem)
        : Vbus::Device("virtio socket"), Virtio::Device(19, bus, irq_ctlr, &config, sizeof(config),
                                                        irq, queue_entries),
          _sem(sem) {
        config.guest_cid = cid;
    }

    void register_callback(Virtio::Callback &callback,
                           Model::Virtio_sock_callback &virtio_soc_callback) {
        _callback = &callback;
        _virtio_sock_callback = &virtio_soc_callback;
    }

    void connect() { _backend_connected = true; }

    void disconnect() { _backend_connected = false; }

    void signal();

    virtual void reset(const VcpuCtx *) override;

    virtual Vbus::Err access(Vbus::Access, const VcpuCtx *, Vbus::Space, mword, uint8,
                             uint64 &) override;

    Virtio::Queue_data const &queue_data_rx() const { return _data[RX]; }
    Virtio::Queue_data const &queue_data_tx() const { return _data[TX]; }
    Virtio::Queue_data const &queue_data_event() const { return _data[EVENT]; }
};
