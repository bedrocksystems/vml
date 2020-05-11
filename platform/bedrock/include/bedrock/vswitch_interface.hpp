/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <zeta/types.hpp>
#include <zeta/zeta.hpp>

#include <errno.hpp>

#include <model/virtio_net.hpp>
#include <vm_config.hpp>

namespace VSwitch {
    struct Queue_info;
    class Interface;
    class Virtio_backend;
    typedef void (*Vswitch_listener)(const Zeta::Zeta_ctx *, Virtio_backend *);
}

struct VSwitch::Queue_info {
    uint32 entries;
    mword desc_memory;
    mword driver_memory;
    mword device_memory;
};

class VSwitch::Interface {

public:
    Interface(const Zeta::Zeta_ctx *_ctx, const Uuid &_server_uuid, uint64 _vmm_base,
              uint64 _guest_base, uint64 _guest_size, Model::Virtio_net *_network, uint16 _port_id,
              Sel sel);

    Errno connect(const Zeta::Zeta_ctx *ctx);

    Sel tx_int_sem(void) { return interrupts; }
    Sel rx_int_sem(void) { return interrupts + 1; }

protected:
    void setup_queue(uint32 idx);
    void uuid_to_mword(const Uuid &uuid, mword &low, mword &high);

protected:
    Model::Virtio_net *network{nullptr};

    bool tx_constructed{false};
    bool rx_constructed{false};

    uint64 features;

private:
    const Uuid client_uuid; // The VMM hyper process UUID
    const Uuid server_uuid; // VSwitch service UUID
    uint64 vmm_base;
    uint64 guest_base;
    uint64 guest_size;

    Queue_info queue_info[2];
    Sel pt_sel;

    Sel interrupts; // Should be allocated as two adjacent selectors. First = Tx interrupt. Second =
                    // Rx Interrupt.

    uint16 port_id;

    Sel _sel;
};

class VSwitch::Virtio_backend : public VSwitch::Interface, public Virtio::Callback {
public:
    Virtio_backend(const Zeta::Zeta_ctx *_ctx, const Uuid &_server_uuid, uint64 _vmm_base,
                   uint64 _guest_base, uint64 _guest_size, Model::Virtio_net *_network,
                   uint16 _port_id, Sel sel, Semaphore *_sem)
        : Interface(_ctx, _server_uuid, _vmm_base, _guest_base, _guest_size, _network, _port_id,
                    sel),
          vswitch_sel(sel), sem(_sem) {}

    void driver_ok() override;

    Errno setup_listeners(const Zeta::Zeta_ctx *ctx);

private:
    static void wait_for_connection(const Zeta::Zeta_ctx *ctx, VSwitch::Virtio_backend *arg);
    [[noreturn]] static void wait_for_tx_int(const Zeta::Zeta_ctx *ctx,
                                             VSwitch::Virtio_backend *arg);
    [[noreturn]] static void wait_for_rx_int(const Zeta::Zeta_ctx *ctx,
                                             VSwitch::Virtio_backend *arg);
    [[noreturn]] static void wait_for_vswitch_signal(const Zeta::Zeta_ctx *ctx,
                                                     VSwitch::Virtio_backend *arg);

    Errno setup_listener(const Zeta::Zeta_ctx *ctx, Vswitch_listener listener, Cpu cpu);

    Sel connection_sm;
    Sel vswitch_sel;
    Semaphore *sem;
};
