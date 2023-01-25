/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/virtio.hpp>
#include <model/virtio_block_defs.hpp>
#include <platform/signal.hpp>
#include <platform/string.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Virtio_block;
    class Virtio_block_callback;
    class Irq_contoller;
}

class Model::Virtio_block_callback {
public:
    virtual void device_reset(const VcpuCtx *ctx) = 0;
    virtual void shutdown(const VcpuCtx *ctx) = 0;
};

class Model::Virtio_block : public Virtio::Device {
private:
    enum { REQUEST = 0 };
    Virtio::Callback *_callback{nullptr};
    Model::Virtio_block_callback *_virtio_block_callback{nullptr};
    Virtio_block_config _config;
    Platform::Signal *_sig;
    bool _backend_connected{false};

    void notify(uint32) override;
    void driver_ok() override;

public:
    struct UserConfig {
        Virtio::Transport *transport{nullptr};
        // 5.2.3 Feature bits
        // VIRTIO_BLK_F_SIZE_MAX (1) Maximum size of any single segment is in size_max.
        // VIRTIO_BLK_F_SEG_MAX (2) Maximum number of segments in a request is in seg_max.
        // Used to constrain the block request size from the guest.
        uint32 device_feature{static_cast<uint32>(Model::VirtioBlockFeatures::SEG_MAX)
                              | static_cast<uint32>(Model::VirtioBlockFeatures::BLK_SIZE_MAX)};
        Model::Virtio_block_config block_config;
    };

    Virtio_block(Irq_controller &irq_ctlr, const Vbus::Bus &bus, uint16 const irq,
                 uint16 const queue_entries, const UserConfig &config, Platform::Signal *sig)
        : Virtio::Device("virtio block", Virtio::DeviceID::BLOCK, bus, irq_ctlr, &_config,
                         sizeof(_config), irq, queue_entries, config.transport,
                         config.device_feature),
          _sig(sig) {
        memcpy(&_config, &config.block_config, sizeof(Model::Virtio_block_config));
    }

    void register_callback(Virtio::Callback &callback,
                           Model::Virtio_block_callback &block_callback) {
        _callback = &callback;
        _virtio_block_callback = &block_callback;
    }

    void connect() { _backend_connected = true; }
    void disconnect() { _backend_connected = false; }
    void signal();

    void reset(const VcpuCtx *) override;
    void shutdown(const VcpuCtx *) override;

    Virtio::DeviceQueue &request_queue() { return device_queue(REQUEST); }
    Virtio::QueueData const &queue_data_request() const { return queue_data(REQUEST); }
};
