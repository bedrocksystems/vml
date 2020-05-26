/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

#include <errno.hpp>
#include <io/console_zeta.hpp>
#include <model/virtio_console.hpp>
#include <pl011/pl011.hpp>
#include <zeta/ec.hpp>
#include <zeta/types.hpp>
#include <zeta/zeta.hpp>

namespace Umx {
    class Connection_helper;
    class Virtio_backend;
    class Pl011_backend;

    enum : size_t {
        GUEST_DEFAULT_TX_SIZE = 31 * 1024, // 31 KB
        GUEST_DEFAULT_RX_SIZE = 1024,      // 1 KB
    };

    constexpr size_t TOTAL_NAME_LEN = 80;

}

/*
 * Conenction_helper is a wrapper around the `Console_zeta` class that allows to
 * establish a connection to UMX from any CPU. UMX currently only runs on CPU 0 but
 * the vmm can run anywhere so a GEC must be created to establish the initial connection.
 * Once done, any CPU can use the connection.
 */
class Umx::Connection_helper {
public:
    Connection_helper()
        : connection_status(ENODEV), console(nullptr), _buff(nullptr),
          _wait_connection(Sels::INVALID) {}
    ~Connection_helper() { deinit(); }

    Errno init(const Zeta::Zeta_ctx *ctx, size_t tx_size = Umx::Connection::DEFAULT_TX_SIZE,
               size_t rx_size = Umx::Connection::DEFAULT_RX_SIZE);
    void deinit();

    Errno setup_umx_bridge(const Uuid &umx_uuid, const char *name);
    void update_connection_status(Errno err);

    Errno connection_status;
    Console_zeta *console;
    Zeta::Global_ec connection_gec;

private:
    struct Connect_info {
        Uuid umx_uuid;
        const char *name;
        Connection_helper *self;
    };

    static void connect(const Zeta::Zeta_ctx *ctx, Connect_info *info);

    uint64 *_buff;
    Sel _wait_connection;
};

class Umx::Virtio_backend : public Virtio::Callback {
public:
    Virtio_backend(Model::Virtio_console *console, Umx::Connection_helper *backend, Semaphore *sem)
        : _backend(backend), _console(console), _sem(sem) {}

    virtual ~Virtio_backend() {}

    void driver_ok() override {}

    Errno setup_umx_virtio_bridge(Cpu cpu, const Uuid &umx_uuid, const char *name);

private:
    [[noreturn]] static void wait_for_input(const Zeta::Zeta_ctx *ctx, Umx::Virtio_backend *arg);
    [[noreturn]] static void wait_for_output(const Zeta::Zeta_ctx *ctx, Umx::Virtio_backend *arg);

    Umx::Connection_helper *_backend;
    Model::Virtio_console *_console;
    Semaphore *_sem;
    Zeta::Global_ec _input_ec, _output_ec;
};

class Umx::Pl011_backend : public Model::Pl011_callback {
public:
    Pl011_backend(Model::Pl011 *console, Umx::Connection_helper *backend)
        : _backend(backend), _console(console) {}
    virtual ~Pl011_backend() {}

    virtual uint32 from_guest_sent(const char &) override;

    Errno setup_umx_pl011_bridge(Cpu cpu, const Uuid &umx_uuid, const char *name);

private:
    [[noreturn]] static void wait_for_input(const Zeta::Zeta_ctx *ctx, Umx::Pl011_backend *arg);

    Umx::Connection_helper *_backend;
    Model::Pl011 *_console;
    Zeta::Global_ec _input_ec;
};
