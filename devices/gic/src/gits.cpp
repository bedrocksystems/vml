/**
 * Copyright (C) 2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <model/gic.hpp>
#include <model/simple_as.hpp>
#include <model/vcpu_types.hpp>
#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/memory.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

enum {
    GITS_CTLR = 0x0,
    GITS_CTLR_END = 0x3,
    GITS_IIDR = 0x4,
    GITS_IIDR_END = 0x7,
    GITS_TYPER = 0x8,
    GITS_TYPER_END = 0xf,
    GITS_CBASER = 0x80,
    GITS_CBASER_END = 0x87,
    GITS_CWRITER = 0x88,
    GITS_CWRITER_END = 0x8F,
    GITS_CREADR = 0x90,
    GITS_CREADR_END = 0x97,
    GITS_BASER = 0x100,
    GITS_BASER_END = 0x13f,
    GITS_PIDR2 = 0xffe8,
    GITS_PIDR2_END = 0xffeb,
};

static constexpr uint8 ARCHREV_GICV3 = 0x30;

static constexpr uint16 GITS_IIDR_IMPLEMENTER = 0x43b;

static constexpr uint64 BASER_DEVICE_TYPE = 1ull << 56;                 // Device type
static constexpr uint64 BASER_INT_COLLECTION_TYPE = 4ull << 56;         // Interrupt collection type
static constexpr uint64 ENTRY_SIZE = 8ull;                              // 8 bytes per entry
static constexpr uint64 BASER_ENTRY_SIZE = (ENTRY_SIZE - 1) << 48;      // minus 1
static constexpr uint64 BASER_RO_MASK = (7ull << 56) | (0x1Full << 48); // RO: Type + Entry Size

Model::Gits::Gits(Vbus::Bus *mem, GicD *distr) : Device("GIC ITS"), _mem_bus(mem), _distr(distr) {
    _baser[0] = BASER_DEVICE_TYPE | BASER_ENTRY_SIZE;         // device table
    _baser[1] = BASER_INT_COLLECTION_TYPE | BASER_ENTRY_SIZE; // interrupt collection table
    // all others are disabled
    _baser[2] = _baser[3] = _baser[4] = _baser[5] = 0;
}

bool
Model::Gits::write_ctlr(uint64 offset, uint8 bytes, uint64 value) {
    ASSERT(offset == 0);
    ASSERT(bytes == 4);
    if ((value & 1u) != 0 && !enabled())
        _ctlr = 1;
    return true;
}

bool
Model::Gits::write_baser(uint8 index, uint64 value) {
    _baser[index] = (value & ~BASER_RO_MASK) | (_baser[index] & BASER_RO_MASK);
    return true;
}

bool
Model::Gits::write_cbaser(uint64 value) {
    _cbaser = value;
    return true;
}

uint64
Model::Gits::read_baser(uint8 index) const {
    return _baser[index];
}

enum ITSCommandType {
    MOVI = 0x01,
    INT = 0x03,
    CLEAR = 0x04,
    SYNC = 0x05,
    MAPD = 0x08,
    MAPC = 0x09,
    MAPTI = 0x0A,
    MAPI = 0x0B,
    INV = 0x0C,
    INVALL = 0x0D,
    MOVALL = 0x0E,
    DISCARD = 0x0F,
};

uint64
Model::Gits::read_device_table(uint32 dev_id) {
    const uint64 dev_table_entry_addr = (_baser[0] & 0xFFFFFFFFF000ul) + static_cast<uint64>(dev_id) * ENTRY_SIZE;
    uint64 device_table_entry = 0;
    if (Errno::NONE
        != Model::SimpleAS::read_bus(*_mem_bus, dev_table_entry_addr, reinterpret_cast<char *>(&device_table_entry),
                                     sizeof(device_table_entry))) {
        WARN("%s: failed to get device entry", __func__);
        return 0;
    }
    if (device_table_entry == 0) {
        WARN("%s: empty device entry", __func__);
        return 0;
    }
    return device_table_entry;
}

void
Model::Gits::write_device_table(uint32 dev_id, uint64 itt_addr) {
    const uint64 dev_table_entry_addr = (_baser[0] & 0xFFFFFFFFF000ul) + static_cast<uint64>(dev_id) * ENTRY_SIZE;
    if (Errno::NONE
        != Model::SimpleAS::write_bus(*_mem_bus, dev_table_entry_addr, reinterpret_cast<const char *>(&itt_addr),
                                      sizeof(itt_addr))) {
        WARN("%s: fail to write Device Table entry 0x%llx", __func__, dev_table_entry_addr);
    }
}

uint64
Model::Gits::read_translation_table(uint64 itt_base, uint32 event_id) {
    const uint64 itt_entry_addr = itt_base + event_id * ENTRY_SIZE;
    uint64 entry_value = 0;
    if (Errno::NONE
        != Model::SimpleAS::read_bus(*_mem_bus, itt_entry_addr, reinterpret_cast<char *>(&entry_value), sizeof(entry_value))) {
        WARN("%s: fail to read ITT entry 0x%llx", __func__, itt_entry_addr);
        return 0;
    }
    return entry_value;
}

void
Model::Gits::write_translation_table(uint64 itt_base, uint32 event_id, uint64 value) {
    const uint64 itt_entry_addr = itt_base + event_id * ENTRY_SIZE;

    if (Errno::NONE
        != Model::SimpleAS::write_bus(*_mem_bus, itt_entry_addr, reinterpret_cast<const char *>(&value), sizeof(value))) {
        WARN("%s: fail to write ITT entry 0x%llx", __func__, itt_entry_addr);
    }
}

uint64
Model::Gits::read_collection_table(uint16 icid) {
    const uint64 ic_table_entry_addr = (_baser[1] & 0xFFFFFFFFF000ul) + static_cast<uint64>(icid) * ENTRY_SIZE;

    uint64 entry_value = 0;

    if (Errno::NONE
        != Model::SimpleAS::read_bus(*_mem_bus, ic_table_entry_addr, reinterpret_cast<char *>(&entry_value),
                                     sizeof(entry_value))) {
        WARN("%s: fail to write Device Table entry 0x%llx", __func__, ic_table_entry_addr);
        return -1ull;
    }

    return entry_value;
}

void
Model::Gits::write_collection_table(uint16 icid, uint64 rd_base) {
    const uint64 ic_table_entry_addr = (_baser[1] & 0xFFFFFFFFF000ul) + static_cast<uint64>(icid) * ENTRY_SIZE;
    if (Errno::NONE
        != Model::SimpleAS::write_bus(*_mem_bus, ic_table_entry_addr, reinterpret_cast<const char *>(&rd_base),
                                      sizeof(rd_base))) {
        WARN("%s: fail to write collection table entry 0x%llx", __func__, ic_table_entry_addr);
        return;
    }
}

void
Model::Gits::handle_movi(uint32 dev_id, uint32 event_id, uint16 icid) {
    const uint64 device_table_entry = read_device_table(dev_id);
    if (device_table_entry == 0ull)
        return;

    const uint64 entry_value = read_translation_table(device_table_entry, event_id);
    if (entry_value == 0ull)
        return;

    const uint16 old_icid = static_cast<uint16>(entry_value >> 32);
    const uint32 pintid = static_cast<uint32>(entry_value);

    uint64 rd_base1 = read_collection_table(old_icid);
    uint64 rd_base2 = read_collection_table(icid);

    if (rd_base1 != rd_base2)
        write_translation_table(device_table_entry, event_id, (static_cast<uint64>(icid) << 32) | pintid);
}

void
Model::Gits::handle_mapd(bool valid, uint32 dev_id, uint64 itt_addr, uint8 itt_size) {
    ASSERT(itt_size == 0);
    ASSERT(dev_id < 0x10000);
    write_device_table(dev_id, valid ? itt_addr : 0);
}
void
Model::Gits::handle_mapc(bool valid, uint32 rd_base, uint16 icid) {
    write_collection_table(icid, valid ? rd_base : -1ull);
}

void
Model::Gits::handle_mapti(uint32 dev_id, uint32 event_id, uint32 pintid, uint16 icid) {
    const uint64 device_table_entry = read_device_table(dev_id);
    if (device_table_entry != 0ull)
        write_translation_table(device_table_entry, event_id, (static_cast<uint64>(icid) << 32) | pintid);
}

void
Model::Gits::handle_command(uint64 q0, uint64 q1, uint64 q2, uint64) {
    const uint8 cmd_type = static_cast<uint8>(q0 & 0xFu);
    const uint32 dev_id = static_cast<uint32>(q0 >> 32);
    const uint8 mapd_size = static_cast<uint8>(q1 & 0x1Fu);
    const uint32 event_id = static_cast<uint32>(q1);
    const uint32 pintid = static_cast<uint32>(q1 >> 32);
    const uint16 icid = static_cast<uint16>(q2);
    const uint32 rd_base = static_cast<uint32>(q2 >> 16);
    const uint64 itt_addr = q2 & 0xFFFFFFFFFFF00ull;
    const bool valid = (q2 >> 63) != 0u;
    switch (cmd_type) {
    case MOVI:
        handle_movi(dev_id, event_id, icid);
        break;
    case INT:
        ASSERT(0);
        break;
    case CLEAR:
        ASSERT(0);
        break;
    case SYNC:
        break;
    case MAPD:
        handle_mapd(valid, dev_id, itt_addr, mapd_size);
        break;
    case MAPC:
        handle_mapc(valid, rd_base, icid);
        break;
    case MAPTI:
        handle_mapti(dev_id, event_id, pintid, icid);
        break;
    case MAPI:
        ASSERT(0);
        break;
    case INV:
        break;
    case INVALL:
        break;
    case MOVALL:
        ASSERT(0);
        break;
    case DISCARD:
        ASSERT(0);
        break;
    default:
        WARN("%s: unknown cmd_type %x", name(), cmd_type);
        ASSERT(0);
        break;
    }
}

void
Model::Gits::fetch_commands() {
    if ((_cbaser >> 63) == 0u)
        return;
    while (_cwriter != _creadr) {
        static constexpr uint64 COMMAND_SIZE = 32;
        const uint64 its_command_addr = (_cbaser & 0xFFFFFFFFFF000ull) + _creadr;
        uint64 its_command[4] = {0, 0, 0, 0};
        if (Errno::NONE
            == Model::SimpleAS::read_bus(*_mem_bus, its_command_addr, reinterpret_cast<char *>(its_command), COMMAND_SIZE)) {
            handle_command(its_command[0], its_command[1], its_command[2], its_command[3]);
        } else {
            _creadr |= 1u; // stalled
            WARN("%s: fail to read ITS command @ 0x%llx ", __func__, its_command_addr);
            break;
        }

        _creadr += COMMAND_SIZE;
        const uint64 cbaser_size = ((_cwriter & 0xFFFull) + 1) * PAGE_SIZE;
        if (_creadr >= cbaser_size)
            _creadr = 0;
    }
}

bool
Model::Gits::mmio_write(uint64 offset, uint8 bytes, uint64 value) {
    switch (offset) {
    case GITS_CTLR ... GITS_CTLR_END:
        return write_ctlr(offset, bytes, value);
    case GITS_CBASER ... GITS_CBASER_END:
        ASSERT(bytes == 8);
        return write_cbaser(value);
    case GITS_CWRITER ... GITS_CWRITER_END:
        ASSERT((value & 1u) == 0u);
        _cwriter = value & 0xFFFE0ull;
        if (enabled())
            fetch_commands();
        return true;
    case GITS_BASER ... GITS_BASER_END:
        ASSERT(bytes == 8);
        return write_baser(static_cast<uint8>((offset - GITS_BASER) / 8u), value);
    default:
        ASSERT(0);
        break;
    }
    return false;
}

bool
Model::Gits::mmio_read(uint64 offset, uint8 bytes, uint64 &value) const {
    switch (offset) {
    case GITS_CTLR ... GITS_CTLR_END:
        return Model::GicD::read_register(offset, GITS_CTLR, GITS_CTLR_END, bytes, _ctlr, value);
    case GITS_IIDR ... GITS_IIDR_END:
        return Model::GicD::read_register(offset, GITS_IIDR, GITS_IIDR_END, bytes, GITS_IIDR_IMPLEMENTER, value);
    case GITS_TYPER ... GITS_TYPER_END:
        return Model::GicD::read_register(offset, GITS_TYPER, GITS_TYPER_END, bytes, 0, value);
    case GITS_CBASER ... GITS_CBASER_END:
        return Model::GicD::read_register(offset, GITS_CBASER, GITS_CBASER_END, bytes, _cbaser, value);
    case GITS_CWRITER ... GITS_CWRITER_END:
        return Model::GicD::read_register(offset, GITS_CWRITER, GITS_CWRITER_END, bytes, _cwriter, value);
    case GITS_CREADR ... GITS_CREADR_END:
        return Model::GicD::read_register(offset, GITS_CREADR, GITS_CREADR_END, bytes, _creadr, value);
    case GITS_BASER ... GITS_BASER_END: {
        uint8 idx = static_cast<uint8>((offset - GITS_BASER) / 8u);
        return Model::GicD::read_register(offset, GITS_BASER + idx * 8, GITS_BASER + idx * 8 + 7, bytes, read_baser(idx), value);
    }
    case GITS_PIDR2 ... GITS_PIDR2_END:
        return Model::GicD::read_register(offset, GITS_PIDR2, GITS_PIDR2_END, bytes, ARCHREV_GICV3, value);

    default:
        ASSERT(0);
        break;
    }

    return true;
}

Vbus::Err
Model::Gits::access(Vbus::Access const access, const VcpuCtx *, Vbus::Space, mword const offset, uint8 const size,
                    uint64 &value) {
    if (access == Vbus::Access::WRITE)
        return mmio_write(offset, size, value) ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;
    if (access == Vbus::Access::READ)
        return mmio_read(offset, size, value) ? Vbus::Err::OK : Vbus::Err::ACCESS_ERR;

    return Vbus::Err::ACCESS_ERR;
}

void
Model::Gits::handle_msi(uint32 event_id, uint32 dev_id) {
    const uint64 device_table_entry = read_device_table(dev_id);
    if (device_table_entry == 0ull)
        return;

    const uint64 entry_value = read_translation_table(device_table_entry, event_id);
    if (entry_value == 0ull)
        return;

    const uint32 pintid = static_cast<uint32>(entry_value);

    const uint16 icid = static_cast<uint16>(entry_value >> 32);

    const uint64 ic_entry = read_collection_table(icid);

    if (ic_entry == -1ull) {
        WARN("%s: invalid rd base", __func__);
        return;
    }
    ASSERT(ic_entry < 16);
    _distr->assert_lpi(pintid, static_cast<uint8>(ic_entry));
}
