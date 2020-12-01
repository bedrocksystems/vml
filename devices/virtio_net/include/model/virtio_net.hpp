/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
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
    VIRTIO_NET_RSC_EXT = (uint64(1) << 61),
    VIRTIO_NET_STANDBY = (uint64(1) << 62),
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
    virtual void device_reset(const Vcpu_ctx *ctx) = 0;
};

class Model::Virtio_net : public Vbus::Device, public Virtio::Device {

private:
    Virtio::Ram const _ram;
    Virtio::Callback *_callback{nullptr};
    Model::Virtio_net_callback *_virtio_net_callback{nullptr};
    Virtio_net_config config __attribute__((aligned(8)));
    Semaphore *_sem;
    bool _backend_connected{false};

    bool mmio_write(Vcpu_id const, uint64 const, uint8 const, uint64 const);
    bool mmio_read(Vcpu_id const, uint64 const, uint8 const, uint64 &) const;

    void _notify(uint32) override;
    void _driver_ok() override;

public:
    Virtio_net(Irq_controller &irq_ctlr, uint64 const guest_base, uint64 const host_base,
               uint64 const size, uint16 const irq, uint16 const queue_entries,
               uint32 const device_feature, uint64 mac, uint16 mtu, Semaphore *sem)
        : Vbus::Device("virtio network"), Virtio::Device(1, _ram, irq_ctlr, &config, sizeof(config),
                                                         irq, queue_entries, device_feature),
          _ram(guest_base, size, host_base), _sem(sem) {
        config.mtu = mtu;
        memcpy(&config.mac, &mac, 6);
    }

    void register_callback(Virtio::Callback &callback,
                           Model::Virtio_net_callback &virtio_net_callback) {
        _callback = &callback;
        _virtio_net_callback = &virtio_net_callback;
    }

    void connect() { _backend_connected = true; }

    void disconnect() { _backend_connected = false; }

    void signal();

    virtual void reset(const Vcpu_ctx *) override;

    virtual Vbus::Err access(Vbus::Access, const Vcpu_ctx *, Vbus::Space, mword, uint8,
                             uint64 &) override;

    Virtio::Queue_data const &queue_data_rx() const { return _data[RX]; }
    Virtio::Queue_data const &queue_data_tx() const { return _data[TX]; }
};
