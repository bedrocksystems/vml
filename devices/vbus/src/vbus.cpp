/**
 * Copyright (C) 2019-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <debug_switches.hpp>
#include <platform/atomic.hpp>
#include <platform/compiler.hpp>
#include <platform/errno.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <platform/rangemap.hpp>
#include <platform/rwlock.hpp>
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
Vbus::Bus::log_trace_info(const Vbus::Bus::DeviceEntry* cur_entry, const Vbus::Bus::DeviceEntry* last_entry, Vbus::Access access,
                          mword addr, uint8 bytes, uint64 val) {
    if (last_entry != cur_entry) {
        if (_fold && _num_accesses > 1 && last_entry != nullptr) {
            INFO("%s accessed %llu times", last_entry->device->name(), _num_accesses.load());
        }
    } else {
        _num_accesses++;
        if (_fold)
            return;
    }

    INFO("%s @ 0x%lx:%u %s " FMTx64, cur_entry->device->name(), addr, bytes, access == EXEC ? "X" : (access == WRITE ? "W" : "R"),
         val);
    _num_accesses = 0;

    return;
}

Vbus::Err
Vbus::Bus::access_with_dev(Device* dev, Vbus::Access access, const VcpuCtx& vcpu_ctx, mword off, uint8 bytes, uint64& val) {
    clock_t start = 0;
    if (Stats::enabled()) {
        dev->accessed();
        start = clock();
    }

    Err err = dev->access(access, &vcpu_ctx, _space, off, bytes, val);
    if (Stats::enabled())
        dev->add_time(static_cast<size_t>(clock() - start));

    return err;
}

Vbus::Err
Vbus::Bus::access(Vbus::Access access, const VcpuCtx& vcpu_ctx, mword addr, uint8 bytes, uint64& val) {
    if (__UNLIKELY__((addr + bytes <= addr)))
        return NO_DEVICE;

    Range<mword> target(addr, bytes);
    const DeviceEntry *entry, *previous_entry = nullptr;

    _vbus_lock.renter();
    entry = _last_access.load(std::memory_order_relaxed);
    if (entry == nullptr || !entry->contains(target)) {
        entry = lookup(addr, bytes);
        if (entry == nullptr) {
            _vbus_lock.rexit();
            return NO_DEVICE;
        }
    }

    if (_trace) {
        previous_entry = _last_access.load(std::memory_order_relaxed);
        if (access == Vbus::READ)
            val = 0ull; // Initialize to zero for logging purposes
    }

    if (_last_access.load(std::memory_order_relaxed) != entry) {
        _last_access.store(entry, std::memory_order_relaxed);
    }

    mword off = _absolute_access ? addr : addr - entry->begin();
    _vbus_lock.rexit();

    Err err = access_with_dev(entry->device, access, vcpu_ctx, off, bytes, val);
    if (_trace)
        log_trace_info(entry, previous_entry, access, addr, bytes, val);

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
Vbus::Bus::reset_device_cb(Vbus::Bus::DeviceEntry* entry, void*) {
    if (entry->device->type() != Device::IRQ_CONTROLLER)
        entry->device->reset();
}

void
Vbus::Bus::reset_irq_ctlr_cb(Vbus::Bus::DeviceEntry* entry, void*) {
    if (entry->device->type() == Device::IRQ_CONTROLLER)
        entry->device->reset();
}

void
Vbus::Bus::reset() const {
    _vbus_lock.renter();
    iter_devices_unlocked(Vbus::Bus::reset_device_cb, static_cast<void*>(nullptr));
    iter_devices_unlocked(Vbus::Bus::reset_irq_ctlr_cb, static_cast<void*>(nullptr));
    _vbus_lock.rexit();
}

void
Vbus::Bus::shutdown_device_cb(Vbus::Bus::DeviceEntry* entry, void*) {
    entry->device->shutdown();
}

void
Vbus::Bus::shutdown() const {
    VERBOSE("Vbus::Bus::shutdown %p", this);
    iter_devices(Vbus::Bus::shutdown_device_cb, static_cast<void*>(nullptr));
}

void
Vbus::Bus::rm_device_cb(RangeNode<mword>* entry) {
    static_cast<Vbus::Bus::DeviceEntry*>(entry)->device->deinit();
}

Errno
Vbus::Bus::deinit() {
    VERBOSE("Vbus::Bus::deinit %p", this);
    _vbus_lock.renter();
    _devices.clear(Vbus::Bus::rm_device_cb);
    _vbus_lock.rexit();

    return Errno::NONE;
}

void
Vbus::Bus::unregister_device(mword addr, mword bytes) {
    Range<mword> range(addr, bytes);

    _vbus_lock.wenter();
    DeviceEntry* rm_dev = static_cast<DeviceEntry*>(_devices.remove(range));

    if (rm_dev == _last_access.load(std::memory_order_relaxed)) {
        _last_access.store(nullptr, std::memory_order_relaxed);
    }
    _vbus_lock.wexit();

    delete rm_dev;
}
