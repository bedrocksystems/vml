/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <platform/log.hpp>
#include <platform/new.hpp>
#include <platform/rangemap.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

const Vbus::Bus::DeviceEntry*
Vbus::Bus::lookup(mword addr, uint64 bytes) const {
    if (__UNLIKELY__((addr + bytes <= addr)))
        return nullptr;

    Range<mword> target(addr, bytes);

    return static_cast<DeviceEntry*>(_devices.lookup(&target));
}

Vbus::Err
Vbus::Bus::access(Vbus::Access access, const VcpuCtx& vcpu_ctx, mword addr, uint8 bytes,
                  uint64& val) {
    const DeviceEntry* entry = lookup(addr, bytes);

    if (entry == nullptr)
        return NO_DEVICE;

    Err err = entry->device->access(access, &vcpu_ctx, _space, addr - entry->begin(), bytes, val);
    if (_trace && entry != nullptr) {
        if (_last_access != entry) {
            if (_fold && _num_accesses > 1) {
                DEBUG("%s accessed %lu times", _last_access->device->name(), _num_accesses);
            }
        } else {
            _num_accesses++;
            if (_fold)
                return err;
        }

        DEBUG("%s @ 0x%lx:%u %s " FMTx64, entry->device->name(), addr, bytes,
              access == EXEC ? "X" : (access == WRITE ? "W" : "R"), val);
        _num_accesses = 0;
        _last_access = entry;
    }

    return err;
}

Vbus::Device*
Vbus::Bus::get_device_at(mword addr, uint64 size) const {
    const DeviceEntry* entry = lookup(addr, size);

    if (entry == nullptr)
        return nullptr;
    return entry->device;
}

[[nodiscard]] bool
Vbus::Bus::register_device(Device* d, mword addr, mword bytes) {
    if (__UNLIKELY__((addr + bytes <= addr)))
        return false;

    Range<mword> range(addr, bytes);
    DeviceEntry* de = new (nothrow) DeviceEntry(d, range);
    if (de == nullptr)
        return false;

    return _devices.insert(de);
}

void
Vbus::Bus::reset_device_cb(Vbus::Bus::DeviceEntry* entry, const VcpuCtx* arg) {
    if (entry->device->type() != Device::IRQ_CONTROLLER)
        entry->device->reset(arg);
}

void
Vbus::Bus::reset_irq_ctlr_cb(Vbus::Bus::DeviceEntry* entry, const VcpuCtx* arg) {
    if (entry->device->type() == Device::IRQ_CONTROLLER)
        entry->device->reset(arg);
}

void
Vbus::Bus::reset(const VcpuCtx& vcpu_ctx) {
    iter_devices(Vbus::Bus::reset_device_cb, &vcpu_ctx);
    iter_devices(Vbus::Bus::reset_irq_ctlr_cb, &vcpu_ctx);
}
