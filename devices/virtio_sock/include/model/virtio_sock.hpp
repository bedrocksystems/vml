/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/virtio.hpp>
#include <platform/signal.hpp>
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

class Model::Virtio_sock : public Virtio::Device {

private:
    enum { RX = 0, TX = 1, EVENT = 2 };
    Virtio::Callback *_callback{nullptr};
    Model::Virtio_sock_callback *_virtio_sock_callback{nullptr};
    Virtio_sock_config _config;
    Platform::Signal *_sig;
    bool _backend_connected{false};

    void notify(uint32) override;
    void driver_ok() override;

public:
    struct UserConfig {
        Virtio::Transport *transport;
        uint64 cid{0};
    };

    Virtio_sock(Irq_controller &irq_ctlr, const Vbus::Bus &bus, uint16 const irq,
                uint16 const queue_entries, const UserConfig &config, Platform::Signal *sig)
        : Virtio::Device("virtio socket", Virtio::DeviceID::SOCKET, bus, irq_ctlr, &_config,
                         sizeof(_config), irq, queue_entries, config.transport),
          _sig(sig) {
        _config.guest_cid = config.cid;
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

    Virtio::QueueData const &queue_data_rx() const { return queue_data(RX); }
    Virtio::QueueData const &queue_data_tx() const { return queue_data(TX); }
    Virtio::QueueData const &queue_data_event() const { return queue_data(EVENT); }
};
