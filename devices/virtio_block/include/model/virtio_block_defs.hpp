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
    enum class VirtioBlockGetID : size_t;
}

enum class Model::VirtioBlockFeatures : uint64 {
    BLK_SIZE_MAX = (1 << 1),
    SEG_MAX = (1 << 2),
    GEMOETRY = (1 << 4),
    RO = (1 << 5),
    BLK_SIZE = (1 << 6),
    FLUSH = (1 << 9),
    TOPOLOGY = (1 << 10),
    CONFIG_WCE = (1 << 11),
    DISCARD = (1 << 13),
    WRITE_ZEROES = (1 << 14),
    LIFETIME = (1 << 15),     // VirtIO v1.2
    SECURE_ERASE = (1 << 16), // VirtIO v1.2
};

enum class Model::VirtioBlockRequestType : uint32 {
    IN = 0,
    OUT = 1,
    FLUSH = 4,
    GET_ID = 8,        // Documented in VirtIO v1.2
    GET_LIFETIME = 10, // Added in VirtIO v1.2. Requires [VirtioBlockFeatures::LIFETIME] negotiation.
    DISCARD = 11,
    WRITE_ZEROES = 13,
    SECURE_ERASE = 14, // Added in VirtIO v1.2. Requires [VirtioBlockFeatures::LIFETIME] negotiation.
};

enum class Model::VirtioBlockStatus : uint8 {
    OK = 0,
    IOERR = 1,
    UNSUPP = 2,
};

enum class Model::VirtioBlockProtocol : size_t {
    SIZE = 512,
};

enum class Model::VirtioBlockGetID : size_t {
    // v1.2, 5.2.6.1 Driver Requirements: Device Operation
    //
    // The length of data MUST be 20 bytes for VIRTIO_BLK_T_GET_ID requests.
    //
    // v1.2, 5.2.6 Device Operation
    //
    // VIRTIO_BLK_T_GET_ID requests fetch the device ID string from the device into data. The device
    // ID string is a NUL-padded ASCII string up to 20 bytes long. If the string is 20 bytes long
    // then there is no NUL terminator.
    DATA_SIZE = 20,

    // Extrapolated from the buffers sent by guest.
    // header (16) + DATA_SIZE (20) + status (1)
    BUFFER_SIZE = 37,
};

#pragma pack(1)
struct Model::Virtio_block_config {
    uint64 capacity{0};
    uint32 size_max{0};
    uint32 seg_max{0};

    struct {
        uint16 cylinder{0};
        uint8 heads{0};
        uint8 sectors{0};
    } geometry;

    uint32 blk_size{0};

    struct {
        uint8 physical_block_exp{0};
        uint8 alignment_offset{0};
        uint16 min_io_size{0};
        uint32 opt_io_size{0};
    } topology;

    uint8 writeback{0};
    uint8 reserved0[3]{0};
    uint32 max_discard_sectors{0};
    uint32 max_discard_seg{0};
    uint32 discard_sector_alignment{0};
    uint32 max_write_zeroes_sectors{0};
    uint32 max_write_zeroes_seg{0};
    uint8 write_zeroes_may_unmap{0};
    uint8 reserved1[3]{0};
};

static_assert(sizeof(Model::Virtio_block_config) == 60);

struct Model::Virtio_block_request_header {
    uint32 type{0};
    uint32 reserved{0};
    uint64 sector{0};
};

static_assert(sizeof(Model::Virtio_block_request_header) == 16);

#pragma pack()

struct Model::Virtio_block_discard_write_zeroes {
    uint64 sector;
    uint32 num_sectors;
    uint32 flags;
};
