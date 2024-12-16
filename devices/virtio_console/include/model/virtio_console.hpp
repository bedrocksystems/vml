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
#include <model/simple_as.hpp>
#include <model/virtio.hpp>
#include <model/virtio_common.hpp>
#include <model/virtio_sg.hpp>
#include <model/virtqueue.hpp>
#include <platform/context.hpp>
#include <platform/errno.hpp>
#include <platform/mutex.hpp>
#include <platform/signal.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class VirtioConsole;
    struct VirtioConsoleConfig;
    class VirtioConsoleCallback;
    class IrqController;
}

struct Model::VirtioConsoleConfig {
    uint16 cols{0};
    uint16 rows{0};
    uint32 num_ports{0};
    uint32 emerg_wr{0};
};

class Model::VirtioConsoleCallback {
public:
    virtual void device_reset() = 0;
    virtual void shutdown() = 0;
};

// NOTE: while [Virtio::Device] is a concrete instance of [Virtio::Sg::Buffer::ChainAccessor],
// it overloads the necessary functions using the
// [Model::SimpleAS::map_guest_mem]/[Model::SimpleAS::unmap_guest_mem] functions. [VirtioConsole],
// however, needs to do demand (un)mapping which means that we must provide custom overrides.
class Model::VirtioConsole : public Virtio::Device, public Virtio::Sg::Buffer::ChainAccessor {
private:
    enum { RX = 0, TX = 1 };
    Model::VirtioConsoleConfig _config;

    Virtio::Sg::Buffer _rx_buff;
    Virtio::Sg::Buffer _tx_buff;
    size_t _tx_buff_progress{0};

    Platform::Signal *_sig_notify_event;
    bool _driver_initialized{false};
    Platform::Signal _sig_notify_empty_space;
    Platform::Mutex _io_lock;

    // [Virtio::Device] overrides
    void notify(uint32) override;
    void driver_ok() override;

    Virtio::Callback *_callback{nullptr};
    Model::VirtioConsoleCallback *_console_callback{nullptr};

public:
    VirtioConsole(IrqController &irq_ctlr, const Vbus::Bus &bus, uint16 const irq, uint16 const queue_entries,
                  Virtio::Transport *transport, Platform::Signal *sig, uint64 device_features = 0)
        : Virtio::Device("virtio console", Virtio::DeviceID::CONSOLE, bus, irq_ctlr, &_config, sizeof(_config), irq,
                         queue_entries, transport, device_features),
          _rx_buff(Virtio::Sg::Buffer(queue_entries)), _tx_buff(Virtio::Sg::Buffer(queue_entries)), _sig_notify_event(sig) {}

    bool init(const Platform_ctx *ctx) {
        if (Errno::NONE != _rx_buff.init())
            return false;
        if (Errno::NONE != _tx_buff.init())
            return false;
        if (not _sig_notify_empty_space.init(ctx))
            return false;
        return _io_lock.init(ctx);
    }

    bool to_guest(const char *buff, size_t size_bytes);
    virtual size_t from_guest(char *out_buf, size_t size_bytes);
    void wait_for_available_buffer() { _sig_notify_empty_space.wait(); }
    void reset() override {
        _rx_buff.conclude_chain_use(device_queue(RX));
        _tx_buff.conclude_chain_use(device_queue(TX));
        _sig_notify_empty_space.sig();
        reset_virtio();

        if (_console_callback != nullptr) {
            _console_callback->device_reset();
        }
    }

    void shutdown() override {
        if (_console_callback != nullptr) {
            _console_callback->shutdown();
        }
    }

    void register_callback(Virtio::Callback *callback, Model::VirtioConsoleCallback *console_callback) {
        _callback = callback;
        _console_callback = console_callback;
    }

private:
    void detach() override {
        Platform::MutexGuard l{_io_lock};
        Model::IOMMUManagedDevice::detach();
    }

    Errno map(const Model::IOMapping &m) override {
        Platform::MutexGuard l{_io_lock};
        return Model::IOMMUManagedDevice::map(m);
    }

    Errno unmap(const Model::IOMapping &m) override {
        Platform::MutexGuard l{_io_lock};
        return Model::IOMMUManagedDevice::unmap(m);
    }

    GPA translate(uint64 addr, size_t size_bytes) {
        if (not use_io_mappings()) {
            return GPA(addr);
        }
        Platform::MutexGuard l{_io_lock};
        return GPA(translate_io(addr, size_bytes));
    }

    // [Virtio::Queue::AddressTranslator] overrides inherited by [Virtio::Sg::Buffer::ChainAccessor]
    Errno vq_addr_to_r_hva(uint64 vqa, size_t size_bytes, char *&hva) override {
        GPA gpa = translate(vqa, size_bytes);
        if (gpa.invalid())
            return Errno::PERM;

        void *temp_hva = nullptr;
        Errno err = Model::SimpleAS::demand_map_bus(*_vbus, gpa, size_bytes, temp_hva, false);

        if (Errno::NONE == err) {
            hva = static_cast<char *>(temp_hva);
        }

        return err;
    }
    Errno vq_addr_to_w_hva(uint64 vqa, size_t size_bytes, char *&hva) override {
        GPA gpa = translate(vqa, size_bytes);
        if (gpa.invalid()) {
            return Errno::PERM;
        }
        void *temp_hva = nullptr;
        Errno err = Model::SimpleAS::demand_map_bus(*_vbus, gpa, size_bytes, temp_hva, true);

        if (Errno::NONE == err) {
            hva = static_cast<char *>(temp_hva);
        }

        return err;
    }
    Errno vq_addr_to_r_hva_post(uint64 vqa, size_t size_bytes, char *hva) override {
        GPA gpa = translate(vqa, size_bytes);
        if (gpa.invalid()) {
            return Errno::PERM;
        }
        return Model::SimpleAS::demand_unmap_bus(*_vbus, gpa, size_bytes, hva);
    }
    Errno vq_addr_to_w_hva_post(uint64 vqa, size_t size_bytes, char *hva) override {
        GPA gpa = translate(vqa, size_bytes);
        if (gpa.invalid()) {
            return Errno::PERM;
        }
        return Model::SimpleAS::demand_unmap_bus_clean(*_vbus, gpa, size_bytes, hva);
    }
};
