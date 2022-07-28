/**
 * Copyright (C) 2022 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <platform/types.hpp>

namespace Model {
    struct Virtio_block_config;
    struct Virtio_block_request_header;
    struct Virtio_block_discard_write_zeroes;
    enum class VirtioBlockFeatures : uint64;
    enum class VirtioBlockRequestType : uint32;
    enum class VirtioBlockStatus : uint8;
    enum class VirtioBlockProtocol : size_t;
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

enum class Model::VirtioBlockProtocol : size_t {
    SIZE = 512,
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

static_assert(sizeof(Model::Virtio_block_request_header) == 16);

#pragma pack()

struct Model::Virtio_block_discard_write_zeroes {
    uint64 sector;
    uint32 num_sectors;
    uint32 flags;
};
