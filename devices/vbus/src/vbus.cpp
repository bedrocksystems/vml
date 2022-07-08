/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <debug_switches.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <platform/rangemap.hpp>
#include <platform/time.hpp>
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
    /*
     * When the size if unknown, consider that this only touching the first device encountered.
     * We will replay the instruction as necessary. Still let the device know that the size of the
     * access is not available at this time.
     */
    const DeviceEntry* entry = lookup(addr, bytes == SIZE_UNKNOWN ? 1 : bytes);

    if (entry == nullptr)
        return NO_DEVICE;

    mword off = _absolute_access ? addr : addr - entry->begin();
    clock_t start = 0;
    if (Stats::enabled()) {
        entry->device->accessed();
        start = clock();
    }

    Err err = entry->device->access(access, &vcpu_ctx, _space, off, bytes, val);

    if (Stats::enabled())
        entry->device->add_time(clock() - start);

    if (_trace && entry != nullptr) {
        if (_last_access != entry) {
            if (_fold && _num_accesses > 1) {
                INFO("%s accessed %lu times", _last_access->device->name(), _num_accesses);
            }
        } else {
            _num_accesses++;
            if (_fold)
                return err;
        }

        INFO("%s @ 0x%lx:%u %s " FMTx64, entry->device->name(), addr, bytes,
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
Vbus::Bus::reset(const VcpuCtx& vcpu_ctx) const {
    iter_devices(Vbus::Bus::reset_device_cb, &vcpu_ctx);
    iter_devices(Vbus::Bus::reset_irq_ctlr_cb, &vcpu_ctx);
}

/*
 * Warning:
 * This function assumes that the caller has a locking scheme for the vbus.
 */
void
Vbus::Bus::unregister_device(mword addr, mword bytes) {
    Range<mword> range(addr, bytes);

    delete _devices.remove(range);
}
