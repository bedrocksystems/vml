/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <debug_switches.hpp>
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

void
Vbus::Bus::log_trace_info(const Vbus::Bus::DeviceEntry* cur_entry, Vbus::Access access, mword addr,
                          uint8 bytes, uint64 val) {
    const DeviceEntry* last_entry = _last_access;

    if (last_entry != cur_entry) {
        if (_fold && _num_accesses > 1 && last_entry != nullptr) {
            INFO("%s accessed %llu times", last_entry->device->name(), _num_accesses.load());
        }
    } else {
        _num_accesses++;
        if (_fold)
            return;
    }

    INFO("%s @ 0x%lx:%u %s " FMTx64, cur_entry->device->name(), addr, bytes,
         access == EXEC ? "X" : (access == WRITE ? "W" : "R"), val);
    _num_accesses = 0;

    return;
}

Vbus::Err
Vbus::Bus::access(Vbus::Access access, const VcpuCtx& vcpu_ctx, mword addr, uint8 bytes,
                  uint64& val) {

    bool absolute_access = _absolute_access; // silly but currently needed to simplify proof

    _vbus_lock.renter();
    /*
     * When the size if unknown, consider that this only touching the first device encountered.
     * We will replay the instruction as necessary. Still let the device know that the size of the
     * access is not available at this time.
     */
    uint8 access_size = (bytes == SIZE_UNKNOWN ? 1 : bytes);
    if (__UNLIKELY__((addr + access_size <= addr)))
        return NO_DEVICE;

    Range<mword> target(addr, access_size);
    const DeviceEntry* entry;
    Device* dev;

    entry = _last_access;
    if (entry == nullptr || !entry->contains(target)) {
        entry = lookup(addr, access_size);
        if (entry == nullptr) {
            _vbus_lock.rexit();
            return NO_DEVICE;
        }
    }

    if (_trace) {
        log_trace_info(entry, access, addr, bytes, val);
    }

    if (_last_access != entry) {
        _last_access = entry;
    }

    dev = entry->device;
    mword off = absolute_access ? addr : addr - entry->begin();
    _vbus_lock.rexit();

    clock_t start = 0;
    if (Stats::enabled()) {
        dev->accessed();
        start = clock();
    }

    Err err = dev->access(access, &vcpu_ctx, _space, off, bytes, val);

    if (Stats::enabled())
        dev->add_time(clock() - start);

    return err;
}

Vbus::Device*
Vbus::Bus::get_device_at(mword addr, uint64 size) const {
    Device* dev;

    _vbus_lock.renter();
    const DeviceEntry* entry = lookup(addr, size);

    if (entry == nullptr) {
        _vbus_lock.rexit();
        return nullptr;
    }

    dev = entry->device;
    _vbus_lock.rexit();

    return dev;
}

[[nodiscard]] bool
Vbus::Bus::register_device(Device* d, mword addr, mword bytes) {
    if (__UNLIKELY__((addr + bytes <= addr)))
        return false;

    Range<mword> range(addr, bytes);
    DeviceEntry* de = new (nothrow) DeviceEntry(d, range);
    if (de == nullptr)
        return false;

    _vbus_lock.wenter();
    auto status = _devices.insert(de);
    _vbus_lock.wexit();
    if (!status)
      delete de;

    return status;
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
    _vbus_lock.renter();
    iter_devices(Vbus::Bus::reset_device_cb, &vcpu_ctx);
    iter_devices(Vbus::Bus::reset_irq_ctlr_cb, &vcpu_ctx);
    _vbus_lock.rexit();
}

void
Vbus::Bus::unregister_device(mword addr, mword bytes) {
    Range<mword> range(addr, bytes);

    _vbus_lock.wenter();
    DeviceEntry* rm_dev = static_cast<DeviceEntry*>(_devices.remove(range));

    if (rm_dev == _last_access) {
        _last_access = nullptr;
    }
    _vbus_lock.wexit();

    delete rm_dev;
}
