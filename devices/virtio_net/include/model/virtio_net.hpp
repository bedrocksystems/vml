/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
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
    class Virtio_net;
    struct Virtio_net_config;
    class Virtio_net_callback;
    class Irq_contoller;
}

enum : uint64 {
    VIRTIO_NET_CSUM = (1 << 0),
    VIRTIO_NET_GUEST_CSUM = (1 << 1),
    VIRTIO_NET_CTRL_GUEST_OFFLOADS = (1 << 2),
    VIRTIO_NET_MTU = (1 << 3),
    VIRTIO_NET_MAC = (1 << 5),
    VIRTIO_NET_GUEST_TSO4 = (1 << 7),
    VIRTIO_NET_GUEST_TSO6 = (1 << 8),
    VIRTIO_NET_GUEST_ECN = (1 << 9),
    VIRTIO_NET_GUEST_UFO = (1 << 10),
    VIRTIO_NET_HOST_TSO4 = (1 << 11),
    VIRTIO_NET_HOST_TSO6 = (1 << 12),
    VIRTIO_NET_HOST_ECN = (1 << 13),
    VIRTIO_NET_HOST_UFO = (1 << 14),
    VIRTIO_NET_MRG_RXBUF = (1 << 15),
    VIRTIO_NET_STATUS = (1 << 16),
    VIRTIO_NET_CTRL_VQ = (1 << 17),
    VIRTIO_NET_CTRL_RX = (1 << 18),
    VIRTIO_NET_CTRL_VLAN = (1 << 19),
    VIRTIO_NET_GUEST_ANNOUNCE = (1 << 21),
    VIRTIO_NET_MQ = (1 << 22),
    VIRTIO_NET_CTRL_MAC_ADDR = (1 << 23),
    VIRTIO_NET_RSC_EXT = 1ull << 61,
    VIRTIO_NET_STANDBY = 1ull << 62,
};

#pragma pack(1)
struct Model::Virtio_net_config {
    uint8 mac[6];
    uint16 status;
    uint16 num_virtqueue_pairs;
    uint16 mtu;
};
#pragma pack()

class Model::Virtio_net_callback {
public:
    virtual void device_reset(const VcpuCtx *ctx) = 0;
    virtual void shutdown(const VcpuCtx *ctx) = 0;
};

class Model::Virtio_net : public Virtio::Device {

private:
    enum { RX = 0, TX = 1 };
    Virtio::Callback *_callback{nullptr};
    Model::Virtio_net_callback *_virtio_net_callback{nullptr};
    Virtio_net_config _config __attribute__((aligned(8)));
    Platform::Signal *_sig;
    bool _backend_connected{false};

    void notify(uint32) override;
    void driver_ok() override;

public:
    struct UserConfig {
        Virtio::Transport *transport;
        uint32 device_feature{0};
        uint64 mac{0};
        uint16 mtu{0};
        uint16 port_id{0};
    };

    Virtio_net(Irq_controller &irq_ctlr, const Vbus::Bus &vbus, uint16 irq,
               uint16 const queue_entries, const UserConfig &config, Platform::Signal *sig)
        : Virtio::Device("virtio network", Virtio::DeviceID::NET, vbus, irq_ctlr, &_config,
                         sizeof(_config), irq, queue_entries, config.transport,
                         config.device_feature),
          _sig(sig) {
        _config.mtu = config.mtu;
        memcpy(&_config.mac, &config.mac, 6);
    }

    void register_callback(Virtio::Callback &callback,
                           Model::Virtio_net_callback &virtio_net_callback) {
        _callback = &callback;
        _virtio_net_callback = &virtio_net_callback;
    }

    void connect() { _backend_connected = true; }

    void disconnect() { _backend_connected = false; }

    void signal();

    void reset(const VcpuCtx *) override;
    void shutdown(const VcpuCtx *) override;

    Virtio::QueueData const &queue_data_rx() const { return queue_data(RX); }
    Virtio::QueueData const &queue_data_tx() const { return queue_data(TX); }
};
