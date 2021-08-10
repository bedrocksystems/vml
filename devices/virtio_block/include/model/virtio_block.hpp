/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/virtio.hpp>
#include <platform/signal.hpp>
#include <platform/string.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Virtio_block;
    struct Virtio_block_config;
    struct Virtio_block_request_header;
    struct Virtio_block_discard_write_zeroes;
    enum class VirtioBlockFeatures : uint64;
    enum class VirtioBlockRequestType : uint32;
    enum class VirtioBlockStatus : uint8;
    class Virtio_block_callback;
    class Irq_contoller;
}

enum class Model::VirtioBlockFeatures : uint64 {
    SIZE_MAX = (1 << 1),
    SEG_MAX = (1 << 2),
    GEMOETRY = (1 << 4),
    RO = (1 << 5),
    BLK_SIZE = (1 << 6),
    FLUSH = (1 << 9),
    TOPOLOGY = (1 << 10),
    CONFIG_WCE = (1 << 11),
    DISCARD = (1 << 13),
    WRITE_ZEROES = (1 << 14),
};

enum class Model::VirtioBlockRequestType : uint32 {
    IN = 0,
    OUT = 1,
    FLUSH = 4,
    DISCARD = 11,
    WRITE_ZEROES = 13,
};

enum class Model::VirtioBlockStatus : uint8 {
    OK = 0,
    IOERR = 1,
    UNSUPP = 2,
};

#pragma pack(1)
struct Model::Virtio_block_config {
    uint64 capacity;
    uint32 size_max;
    uint32 seg_max;

    struct {
        uint16 cylinder;
        uint8 heads;
        uint8 sectors;
    } geometry;

    uint32 blk_size;

    struct {
        uint8 physical_block_exp;
        uint8 alignment_offset;
        uint16 min_io_size;
        uint32 opt_io_size;
    } topology;

    uint8 writeback;
    uint8 reserved0[3];
    uint32 max_discard_sectors;
    uint32 max_discard_seg;
    uint32 discard_sector_alignment;
    uint32 max_write_zeroes_sectors;
    uint32 max_write_zeroes_seg;
    uint8 write_zeroes_may_unmap;
    uint8 reserved1[3];
};

static_assert(sizeof(Model::Virtio_block_config) == 60);
struct Model::Virtio_block_request_header {
    uint32 type;
    uint32 reserved;
    uint64 sector;
};

#pragma pack()

struct Model::Virtio_block_discard_write_zeroes {
    uint64 sector;
    uint32 num_sectors;
    uint32 flags;
};

class Model::Virtio_block_callback {
public:
    virtual void device_reset(const VcpuCtx *ctx) = 0;
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
        uint32 device_feature{0};
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

    virtual void reset(const VcpuCtx *) override;

    Virtio::DeviceQueue &request_queue() { return device_queue(REQUEST); }
    Virtio::QueueData const &queue_data_request() const { return queue_data(REQUEST); }
};
