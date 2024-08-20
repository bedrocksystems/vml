/**
 * Copyright (C) 2021-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#pragma once

#include <model/iommu_interface.hpp>
#include <model/irq_controller.hpp>
#include <model/vcpu_types.hpp>
#include <model/virtio.hpp>
#include <model/virtio_block_defs.hpp>
#include <model/virtio_common.hpp>
#include <model/virtqueue.hpp>
#include <platform/errno.hpp>
#include <platform/signal.hpp>
#include <platform/string.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class VirtioBlock;
    class VirtioBlockCallback;
    class Irq_contoller;
}

class Model::VirtioBlockCallback {
public:
    virtual void device_reset(const VcpuCtx *ctx) = 0;
    virtual void shutdown() = 0;

    // IOMMU callbacks
    virtual void attach() = 0;
    virtual void detach() = 0;
    virtual Errno map(const Model::IOMapping &m) = 0;
    virtual Errno unmap(const Model::IOMapping &m) = 0;
};

class Model::VirtioBlock : public Virtio::Device {
private:
    enum { REQUEST = 0 };
    Virtio::Callback *_callback{nullptr};
    Model::VirtioBlockCallback *_virtio_block_callback{nullptr};
    VirtioBlockConfig _config;
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
        uint64 device_feature{static_cast<uint64>(Model::VirtioBlockFeatures::SEG_MAX)
                              | static_cast<uint64>(Model::VirtioBlockFeatures::BLK_SIZE_MAX)};
        Model::VirtioBlockConfig block_config;
    };

    VirtioBlock(IrqController &irq_ctlr, const Vbus::Bus &bus, uint16 const irq, uint16 const queue_entries,
                const UserConfig &config, Platform::Signal *sig)
        : Virtio::Device("virtio block", Virtio::DeviceID::BLOCK, bus, irq_ctlr, &_config, sizeof(_config), irq, queue_entries,
                         config.transport, config.device_feature),
          _sig(sig) {
        memcpy(&_config, &config.block_config, sizeof(Model::VirtioBlockConfig));
    }

    void register_callback(Virtio::Callback &callback, Model::VirtioBlockCallback &block_callback) {
        _callback = &callback;
        _virtio_block_callback = &block_callback;
    }

    void connect() { _backend_connected = true; }
    void disconnect() { _backend_connected = false; }
    void signal();

    void reset(const VcpuCtx *) override;
    void shutdown() override;

    // Override [Model::IOMMUManagedDevice] interfaces
    void attach() override;
    void detach() override;
    Errno map(const Model::IOMapping &m) override;
    Errno unmap(const Model::IOMapping &m) override;

    Virtio::DeviceQueue &request_queue() { return device_queue(REQUEST); }
    Virtio::QueueData const &queue_data_request() const { return queue_data(REQUEST); }
};
