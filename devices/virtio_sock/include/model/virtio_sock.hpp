/**
 * Copyright (C) 2021-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#pragma once

#include <model/iommu_interface.hpp>
#include <model/irq_controller.hpp>
#include <model/virtio.hpp>
#include <model/virtio_common.hpp>
#include <platform/errno.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class VirtioSock;
    struct VirtioSockConfig;
    class VirtioSockCallback;
    class Irq_contoller;
}

struct Model::VirtioSockConfig {
    uint64 guest_cid{UINT64_MAX};
};

class Model::VirtioSockCallback {
public:
    virtual void device_reset() = 0;
    virtual void shutdown() = 0;

    // IOMMU callbacks
    virtual void attach() = 0;
    virtual void detach() = 0;
    virtual Errno map(const Model::IOMapping &m) = 0;
    virtual Errno unmap(const Model::IOMapping &m) = 0;
};

class Model::VirtioSock : public Virtio::Device {

private:
    enum { RX = 0, TX = 1, EVENT = 2 };
    Virtio::Callback *_callback{nullptr};
    Model::VirtioSockCallback *_virtio_sock_callback{nullptr};
    VirtioSockConfig _config;
    Platform::Signal *_sig;
    bool _backend_connected{false};

    void notify(uint32) override;
    void driver_ok() override;

public:
    struct UserConfig {
        Virtio::Transport *transport{nullptr};
        uint64 cid{0};
        uint64 device_features{0};
    };

    VirtioSock(IrqController &irq_ctlr, const Vbus::Bus &bus, uint16 const irq, uint16 const queue_entries,
               const UserConfig &config, Platform::Signal *sig)
        : Virtio::Device("virtio socket", Virtio::DeviceID::SOCKET, bus, irq_ctlr, &_config, sizeof(_config), irq, queue_entries,
                         config.transport, config.device_features),
          _sig(sig) {
        _config.guest_cid = config.cid;
    }

    void register_callback(Virtio::Callback &callback, Model::VirtioSockCallback &virtio_soc_callback) {
        _callback = &callback;
        _virtio_sock_callback = &virtio_soc_callback;
    }

    void connect() { _backend_connected = true; }

    void disconnect() { _backend_connected = false; }

    void signal();

    void reset() override;
    void shutdown() override;

    // Override [Model::IOMMUManagedDevice] interfaces
    void attach() override;
    void detach() override;
    Errno map(const Model::IOMapping &m) override;
    Errno unmap(const Model::IOMapping &m) override;

    Virtio::QueueData const &queue_data_rx() const { return queue_data(RX); }
    Virtio::QueueData const &queue_data_tx() const { return queue_data(TX); }
    Virtio::QueueData const &queue_data_event() const { return queue_data(EVENT); }
};
