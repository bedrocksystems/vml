/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/virtio.hpp>
#include <model/virtio_sg.hpp>
#include <platform/semaphore.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Virtio_console;
    struct Virtio_console_config;
    class VirtioConsoleCallback;
    class Irq_controller;
}

struct Model::Virtio_console_config {
    uint16 cols;
    uint16 rows;
    uint32 num_ports;
    uint32 emerg_wr;
};

class Model::VirtioConsoleCallback {
public:
    virtual void device_reset(const VcpuCtx *ctx) = 0;
    virtual void shutdown() = 0;
};

// NOTE: while [Virtio::Device] is a concrete instance of [Virtio::Sg::Buffer::ChainAccessor],
// it overloads the necessary functions using the
// [Model::SimpleAS::map_guest_mem]/[Model::SimpleAS::unmap_guest_mem] functions. [Virtio_console],
// however, needs to do demand (un)mapping which means that we must provide custom overrides.
class Model::Virtio_console : public Virtio::Device {
private:
    enum { RX = 0, TX = 1 };
    Model::Virtio_console_config _config __attribute__((aligned(8)));

    Virtio::Sg::Buffer _rx_buff;
    Virtio::Sg::Buffer _tx_buff;
    size_t _tx_buff_progress{0};

    Platform::Signal *_sig_notify_event;
    bool _driver_initialized{false};
    Platform::Signal _sig_notify_empty_space;

    // [Virtio::Device] overrides
    void notify(uint32) override;
    void driver_ok() override;

    Virtio::Callback *_callback{nullptr};
    Model::VirtioConsoleCallback *_console_callback{nullptr};

public:
    Virtio_console(Irq_controller &irq_ctlr, const Vbus::Bus &bus, uint16 const irq,
                   uint16 const queue_entries, Virtio::Transport *transport, Platform::Signal *sig)
        : Virtio::Device("virtio console", Virtio::DeviceID::CONSOLE, bus, irq_ctlr, &_config,
                         sizeof(_config), irq, queue_entries, transport),
          _rx_buff(Virtio::Sg::Buffer(queue_entries)), _tx_buff(Virtio::Sg::Buffer(queue_entries)),
          _sig_notify_event(sig) {}

    bool init(const Platform_ctx *ctx) {
        if (Errno::NONE != _rx_buff.init())
            return false;
        if (Errno::NONE != _tx_buff.init())
            return false;
        return _sig_notify_empty_space.init(ctx);
    }

    bool to_guest(const char *buff, size_t size_bytes);
    virtual size_t from_guest(char *out_buf, size_t size_bytes);
    void wait_for_available_buffer() { _sig_notify_empty_space.wait(); }
    void reset(const VcpuCtx *ctx) override {
        _rx_buff.conclude_chain_use(device_queue(RX));
        _tx_buff.conclude_chain_use(device_queue(TX));
        _sig_notify_empty_space.sig();
        reset_virtio();

        if (_console_callback != nullptr) {
            _console_callback->device_reset(ctx);
        }
    }

    void shutdown() override {
        if (_console_callback != nullptr) {
            _console_callback->shutdown();
        }
    }

    void register_callback(Virtio::Callback *callback,
                           Model::VirtioConsoleCallback *console_callback) {
        _callback = callback;
        _console_callback = console_callback;
    }

    // [GuestPhysicalToVirtual] overrides inherited by [Virtio::Sg::Buffer::ChainAccessor]
    //
    // NOTE: mapping depends on whether or not the the va will be used for reads or writes,
    // but unmapping is done unconditionally.
    Errno gpa_to_va(const GPA &gpa, size_t size_bytes, char *&va) override {
        void *temp_va = nullptr;
        Errno err = Model::SimpleAS::demand_map_bus(*_vbus, gpa, size_bytes, temp_va, false);

        if (Errno::NONE == err) {
            va = static_cast<char *>(temp_va);
        }

        return err;
    }
    Errno gpa_to_va_write(const GPA &gpa, size_t size_bytes, char *&va) override {
        void *temp_va = nullptr;
        Errno err = Model::SimpleAS::demand_map_bus(*_vbus, gpa, size_bytes, temp_va, true);

        if (Errno::NONE == err) {
            va = static_cast<char *>(temp_va);
        }

        return err;
    }
    Errno gpa_to_va_post(const GPA &gpa, size_t size_bytes, char *va) override {
        return Model::SimpleAS::demand_unmap_bus(*_vbus, gpa, size_bytes, va);
    }
    Errno gpa_to_va_post_write(const GPA &gpa, size_t size_bytes, char *va) override {
        return Model::SimpleAS::demand_unmap_bus_clean(*_vbus, gpa, size_bytes, va);
    }
};
