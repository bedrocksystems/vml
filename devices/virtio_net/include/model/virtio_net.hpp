/**
 * Copyright (C) 2019-2024 BlueRock Security, Inc.
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
#include <platform/compiler.hpp>
#include <platform/errno.hpp>
#include <platform/signal.hpp>
#include <platform/string.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class VirtioNet;
    struct VirtioNetConfig;
    class VirtioNetCallback;
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
struct Model::VirtioNetConfig {
    VirtioNetConfig() = default;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init) - tidy misses the memcpy
    VirtioNetConfig(const uint8 *pmac, uint16 pmtu) : mtu(pmtu) { memcpy(mac, pmac, ARRAY_LENGTH(mac)); }

    uint8 mac[6] = {0};
    uint16 status{0};
    uint16 num_virtqueue_pairs{0};
    uint16 mtu{0};
};
#pragma pack()

class Model::VirtioNetCallback {
public:
    virtual void device_reset() = 0;
    virtual void shutdown() = 0;

    // IOMMU callbacks
    virtual void attach() = 0;
    virtual void detach() = 0;
    virtual Errno map(const Model::IOMapping &m) = 0;
    virtual Errno unmap(const Model::IOMapping &m) = 0;
};

class Model::VirtioNet : public Virtio::Device {

private:
    enum { RX = 0, TX = 1 };
    Virtio::Callback *_callback{nullptr};
    Model::VirtioNetCallback *_virtio_net_callback{nullptr};
    VirtioNetConfig _config;
    Platform::Signal *_sig;
    bool _backend_connected{false};

    void notify(uint32) override;
    void driver_ok() override;

public:
    struct UserConfig {
        Virtio::Transport *transport{nullptr};
        uint64 device_feature{0};
        uint64 mac{0};
        uint16 mtu{0};
        uint16 port_id{0};
    };

    VirtioNet(IrqController &irq_ctlr, const Vbus::Bus &vbus, uint16 irq, uint16 const queue_entries, const UserConfig &config,
              Platform::Signal *sig)
        : Virtio::Device("virtio network", Virtio::DeviceID::NET, vbus, irq_ctlr, &_config, sizeof(_config), irq, queue_entries,
                         config.transport, config.device_feature),
          _config{reinterpret_cast<const uint8 *>(&config.mac), config.mtu}, _sig(sig) {}

    void register_callback(Virtio::Callback &callback, Model::VirtioNetCallback &virtio_net_callback) {
        _callback = &callback;
        _virtio_net_callback = &virtio_net_callback;
    }

    void connect() { _backend_connected = true; }

    void disconnect() { _backend_connected = false; }

    void signal();

    void reset() override;
    void shutdown() override;

    Errno deinit() override { return Errno::NONE; }

    // Override [Model::IOMMUManagedDevice] interfaces
    void attach() override;
    void detach() override;
    Errno map(const Model::IOMapping &m) override;
    Errno unmap(const Model::IOMapping &m) override;

    Virtio::QueueData const &queue_data_rx() const { return queue_data(RX); }
    Virtio::QueueData const &queue_data_tx() const { return queue_data(TX); }

    void get_device_specific_config(Model::VirtioNetConfig &config) const { config = _config; }
};
