/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <alloc/heap.hpp>
#include <alloc/sels.hpp>
#include <alloc/vmap.hpp>
#include <bedrock/umx_interface.hpp>
#include <io/console_zeta.hpp>
#include <log/log.hpp>
#include <string.hpp>
#include <zeta/zeta.hpp>

Errno
Umx::Connection_helper::init(const Zeta::Zeta_ctx *ctx, size_t tx_size, size_t rx_size) {
    const size_t pages = (tx_size + rx_size + PAGE_SIZE - 1) >> PAGE_BITS;
    _buff = static_cast<uint64 *>(pagealloc(pages));
    Errno ret;

    if (!_buff) {
        return ENOMEM;
    }

    _wait_connection = Sels::alloc();
    if (_wait_connection == Sels::INVALID) {
        deinit();
        return ENOMEM;
    }

    console = new Console_zeta(_buff, Sels::alloc(2, 1), tx_size, rx_size);
    if (!console) {
        deinit();
        return ENOMEM;
    }

    ret = Zeta::create_sm(ctx, _wait_connection);
    if (ret != Errno::ENONE) {
        deinit();
        return ret;
    }

    ret = console->initialize(ctx);
    if (ret != Errno::ENONE) {
        deinit();
        return ret;
    }

    return ret;
}

void
Umx::Connection_helper::deinit() {
    if (console != nullptr) {
        delete console;
        console = nullptr;
    }
    if (_buff != nullptr) {
        delete _buff;
        _buff = nullptr;
    }
}

void
Umx::Connection_helper::update_connection_status(Errno err) {
    connection_status = err;
    Zeta::sm_up(_wait_connection);
}

void
Umx::Connection_helper::connect(const Zeta::Zeta_ctx *ctx, Connect_info *info) {
    Sel umx_pt = Sels::alloc();
    Console_zeta *console = info->self->console;

    if (umx_pt == Sels::INVALID) {
        WARN("Unable to allocate a sel to connect to UMX");
        info->self->update_connection_status(ENOMEM);
        return;
    }

    Errno err = console->bind(ctx, info->umx_uuid, umx_pt, ctx->cpu());
    if (err != ENONE) {
        WARN("Unable to bind UMX");
        info->self->update_connection_status(err);
        return;
    }

    err = console->connect(ctx, info->name);
    if (err != ENONE) {
        WARN("Unable to connect to UMX");
    }
    // Signal no matter what to unblock the caller
    info->self->update_connection_status(err);
}

Errno
Umx::Connection_helper::setup_umx_bridge(const Uuid &umx_uuid, const char *name) {
    constexpr ::Cpu cpu = 0; /* For now, UMX only is on CPU0 */
    Connect_info info{umx_uuid, name, this};
    Errno err = connection_gec.start(
        cpu, Nova::Qpd(), reinterpret_cast<Zeta::global_ec_entry>(Connection_helper::connect),
        &info);
    if (err != ENONE)
        return err;

    err = Zeta::sm_down(_wait_connection);
    if (err != ENONE)
        return err;

    return connection_status;
}

static void
device_format_name(char dst[Umx::TOTAL_NAME_LEN], const char *device_name,
                   const char *user_friendly_name) {
    size_t device_name_len = strnlen(device_name, Umx::TOTAL_NAME_LEN - 1);
    ASSERT(device_name_len > 0);
    size_t user_name_len = 0;

    if (user_friendly_name) {
        size_t max_len = Umx::TOTAL_NAME_LEN - device_name_len - 2;
        user_name_len = strnlen(user_friendly_name, max_len);
        // Put the VM name before the device name
        strncpy(dst, user_friendly_name, user_name_len);
        dst[user_name_len++] = ' ';
        dst[user_name_len] = '\0';
    }

    strncpy(dst + user_name_len, device_name, Umx::TOTAL_NAME_LEN - user_name_len);

    dst[Umx::TOTAL_NAME_LEN - 1] = '\0';
}

void
Umx::Virtio_backend::wait_for_input(const Zeta::Zeta_ctx *, Virtio_backend *virtio) {
    while (1) {
        char byte;
        if (virtio->_backend->console->read(byte)) {
            virtio->_console->to_guest(&byte, 1);
        }
    }
}

void
Umx::Virtio_backend::wait_for_output(const Zeta::Zeta_ctx *, Virtio_backend *virtio) {
    while (1) {
        uint32 size;

        virtio->_sem->acquire();

        char *buffer = virtio->_console->from_guest(size);
        if (not buffer)
            continue;

        for (unsigned i = 0; i < size; i++) {
            char cvalue = buffer[i];
            virtio->_backend->console->write(cvalue);
        }
        virtio->_backend->console->flush();
        virtio->_console->release_buffer();
    }
}

Errno
Umx::Virtio_backend::setup_umx_virtio_bridge(Cpu cpu, const Uuid &umx_uuid, const char *name) {
    char dst[Umx::TOTAL_NAME_LEN];

    device_format_name(dst, "virtio console", name);

    Errno err = _backend->setup_umx_bridge(umx_uuid, dst);
    if (err != ENONE)
        return err;

    // Disable the console from the list of Zeta consoles - we are driving it manually
    _backend->console->disable();

    err = _input_ec.start(cpu, Nova::Qpd(),
                          reinterpret_cast<Zeta::global_ec_entry>(Virtio_backend::wait_for_input),
                          this);
    if (err != ENONE)
        return err;

    return _output_ec.start(
        cpu, Nova::Qpd(), reinterpret_cast<Zeta::global_ec_entry>(Virtio_backend::wait_for_output),
        this);
}

uint32
Umx::Pl011_backend::from_guest_sent(const char &c) {
    _backend->console->write(c);
    _backend->console->flush();

    return 1;
}

void
Umx::Pl011_backend::wait_for_input(const Zeta::Zeta_ctx *, Pl011_backend *pl011) {
    while (1) {
        char byte;
        if (pl011->_backend->console->read(byte)) {
            pl011->_console->to_guest(&byte, 1);
        }
    }
}

Errno
Umx::Pl011_backend::setup_umx_pl011_bridge(Cpu cpu, const Uuid &umx_uuid, const char *name) {
    char dst[Umx::TOTAL_NAME_LEN];

    device_format_name(dst, "pl011 console", name);
    Errno err = _backend->setup_umx_bridge(umx_uuid, dst);
    if (err != ENONE)
        return err;
    // Disable the console from the list of Zeta console - we are driving it manually
    _backend->console->disable();

    return _input_ec.start(cpu, Nova::Qpd(),
                           reinterpret_cast<Zeta::global_ec_entry>(Pl011_backend::wait_for_input),
                           this);
}
