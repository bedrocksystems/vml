/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */

#include <alloc/sels.hpp>
#include <alloc/vmap.hpp>
#include <bedrock/vswitch_interface.hpp>
#include <log/log.hpp>
#include <zeta/zeta.hpp>

namespace VSwitch {

    Interface::Interface(const Zeta::Zeta_ctx *ctx, const Uuid &_server_uuid, uint64 _vmm_base,
                         uint64 _guest_base, uint64 _guest_size, Model::Virtio_net *_network,
                         uint16 _port_id, Sel sel)
        : network(_network), server_uuid(_server_uuid), vmm_base(_vmm_base),
          guest_base(_guest_base), guest_size(_guest_size), port_id(_port_id), _sel(sel) {
        pt_sel = Sels::alloc();
        ASSERT(pt_sel != Sels::INVALID);

        interrupts = Sels::alloc(2, 1);
        ASSERT(interrupts != Sels::INVALID);

        Errno ret = Zeta::create_sm(ctx, interrupts);
        ASSERT(ret == Errno::ENONE);

        ret = Zeta::create_sm(ctx, interrupts + 1);
        ASSERT(ret == Errno::ENONE);
    }

    void Interface::setup_queue(uint32 queue_idx) {
        auto queue_data = (queue_idx == 0) ? network->queue_data_tx() : network->queue_data_rx();

        mword desc_memory = queue_data.descr_low | (uint64(queue_data.descr_high) << 32);
        mword driver_memory = queue_data.driver_low | (uint64(queue_data.driver_high) << 32);
        mword device_memory = queue_data.device_low | (uint64(queue_data.device_high) << 32);

        queue_info[queue_idx].desc_memory = desc_memory;
        queue_info[queue_idx].driver_memory = driver_memory;
        queue_info[queue_idx].device_memory = device_memory;
        queue_info[queue_idx].entries = queue_data.num;
    }

    Errno Interface::connect(const Zeta::Zeta_ctx *ctx) {
        Errno ret;

        if (server_uuid == Uuid::NULL) {
            WARN("vSwitch UUID invalid.");
            return Errno::EINVAL;
        }

        ret = Zeta::Service::connect(ctx, server_uuid, ctx->cpu(), pt_sel);
        if (ret != Errno::ENONE) {
            WARN("Connection to vSwitch timedout. Exiting with failure.");
            return ret;
        }

        // Share interrupt semaphores.
        ret = Zeta::Service::share(ctx, pt_sel, 0, Nova::Obj_crd(interrupts, 1));
        if (ret != Errno::ENONE)
            return ret;

        size_t num_pages = numpages(guest_size);
        ret = Zeta::Service::share(
            ctx, pt_sel, 1,
            Nova::Mem_crd(vmm_base, uint8(order_up(num_pages)), Nova::Mem_cred(true, true, false)));
        if (ret != Errno::ENONE)
            return ret;

        ctx->utcb()->mset(0, 0); // connect

        ctx->utcb()->mset(1, queue_info[0].entries);
        ctx->utcb()->mset(2, queue_info[0].desc_memory);
        ctx->utcb()->mset(3, queue_info[0].driver_memory);
        ctx->utcb()->mset(4, queue_info[0].device_memory);

        ctx->utcb()->mset(5, queue_info[1].entries);
        ctx->utcb()->mset(6, queue_info[1].desc_memory);
        ctx->utcb()->mset(7, queue_info[1].driver_memory);
        ctx->utcb()->mset(8, queue_info[1].device_memory);

        ctx->utcb()->mset(9, features);
        ctx->utcb()->mset(10, guest_base);
        ctx->utcb()->mset(11, guest_size);

        ctx->utcb()->mset(12, 0); // uuid_low for future implementation
        ctx->utcb()->mset(13, 0); // uuid_high for future implementation

        ctx->utcb()->mset(14, static_cast<mword>(port_id));

        Nova::Mtd mtd = 15;
        ret = Zeta::ipc_call(pt_sel, mtd);
        if (ret != Errno::ENONE)
            return ret;

        return Zeta::Service::import(ctx, pt_sel, 2, Nova::Obj_crd(_sel, 0));
    }

    void Virtio_backend::driver_ok() {
        setup_queue(0);
        setup_queue(1);

        features = network->drv_feature();
        Zeta::sm_up(connection_sm);
    }

    void Virtio_backend::wait_for_connection(const Zeta::Zeta_ctx *ctx,
                                             VSwitch::Virtio_backend *backend) {
        ASSERT(backend != nullptr);

        Zeta::sm_down(backend->connection_sm);
        DEBUG("Connecting to vSwitch.");
        if (backend->connect(ctx) == Errno::ENONE) {
            DEBUG("Connection to vSwitch successful.");
            backend->network->connect();
        } else {
            WARN("Connection to vSwitch failed.");
        }
    }

    void Virtio_backend::wait_for_tx_int(const Zeta::Zeta_ctx *, VSwitch::Virtio_backend *backend) {
        ASSERT(backend != nullptr);
        ASSERT(backend->network != nullptr);

        while (1) {
            Zeta::sm_down(backend->tx_int_sem(), 0, true);
            backend->network->signal();
        }
    }

    void Virtio_backend::wait_for_rx_int(const Zeta::Zeta_ctx *, VSwitch::Virtio_backend *backend) {
        ASSERT(backend != nullptr);
        ASSERT(backend->network != nullptr);

        while (1) {
            Zeta::sm_down(backend->rx_int_sem(), 0, true);
            backend->network->signal();
        }
    }

    void Virtio_backend::wait_for_vswitch_signal(const Zeta::Zeta_ctx *,
                                                 VSwitch::Virtio_backend *backend) {
        ASSERT(backend != nullptr);
        ASSERT(backend->network != nullptr);

        while (1) {
            backend->sem->acquire();
            Zeta::sm_up(backend->vswitch_sel);
        }
    }

    Errno Virtio_backend::setup_listener(Vswitch_listener listener, Cpu cpu) {
        return gec.start(cpu, Nova::Qpd(), reinterpret_cast<Zeta::global_ec_entry>(listener), this);
    }

    Errno Virtio_backend::setup_listeners(const Zeta::Zeta_ctx *ctx) {
        Errno err;

        connection_sm = Sels::alloc();
        if (connection_sm == Sels::INVALID) {
            return ENOMEM;
        }

        err = Zeta::create_sm(ctx, connection_sm);
        if (err != Errno::ENONE) {
            WARN("create_sm failed!");
            return err;
        }

        err = setup_listener(wait_for_connection, 0);
        if (err != Errno::ENONE) {
            WARN("connection listener creation failed");
            return err;
        }

        err = setup_listener(wait_for_tx_int, ctx->cpu());
        if (err != Errno::ENONE) {
            WARN("tx int listener creation failed");
            return err;
        }

        err = setup_listener(wait_for_rx_int, ctx->cpu());
        if (err != Errno::ENONE) {
            WARN("rx int listener creation failed");
            return err;
        }

        err = setup_listener(wait_for_vswitch_signal, ctx->cpu());
        if (err != Errno::ENONE) {
            WARN("rx int listener creation failed");
            return err;
        }

        return Errno::ENONE;
    }
}
