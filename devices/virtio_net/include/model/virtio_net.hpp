/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <model/gic.hpp>
#include <model/virtio.hpp>
#include <platform/semaphore.hpp>
#include <platform/string.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Virtio_net;
    struct Virtio_net_config;
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

class Model::Virtio_net : public Vbus::Device, public Virtio::Device {

private:
    Virtio::Ram const _ram;
    Virtio::Callback *_callback{nullptr};
    Virtio_net_config config __attribute__((aligned(8)));
    Semaphore *_sem;
    bool _backend_connected{false};

    bool mmio_write(Vcpu_id const, uint64 const, uint8 const, uint64 const);
    bool mmio_read(Vcpu_id const, uint64 const, uint8 const, uint64 &) const;

    void _notify(uint32) override;
    void _driver_ok() override;

public:
    Virtio_net(Gic_d &gic, uint64 const guest_base, uint64 const host_base, uint64 const size,
               uint16 const irq, uint16 const queue_entries, uint32 const device_feature,
               uint64 mac, uint16 mtu, Semaphore *sem)
        : Vbus::Device("virtio network"), Virtio::Device(1, _ram, gic, &config, sizeof(config), irq,
                                                         queue_entries, device_feature),
          _ram(guest_base, size, host_base), _sem(sem) {
        config.mtu = mtu;
        memcpy(&config.mac, &mac, 6);
    }

    void register_callback(Virtio::Callback &callback) { _callback = &callback; }

    void connect() { _backend_connected = true; }

    void disconnect() { _backend_connected = false; }

    void signal();

    virtual void reset() override { _reset(); }

    virtual Vbus::Err access(Vbus::Access, const Vcpu_ctx *, mword, uint8, uint64 &) override;

    Virtio::Queue_data const &queue_data_rx() const { return _data[RX]; }
    Virtio::Queue_data const &queue_data_tx() const { return _data[TX]; }
};
