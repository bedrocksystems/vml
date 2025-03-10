/**
 * Copyright (C) 2023-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <intrusive/map.hpp>
#include <model/vcpu_types.hpp>
#include <msr/msr_base.hpp>
#include <platform/compiler.hpp>
#include <platform/log.hpp>
#include <platform/time.hpp>
#include <platform/types.hpp>
#include <vbus/vbus.hpp>

[[nodiscard]] bool
Msr::BaseBus::register_device(RegisterBase *r, mword id) {
    RegisterBase *old = _devices.insert(id, r);

    if (__UNLIKELY__(old != nullptr)) {
        _devices.remove_existing(r);
        _devices.insert(id, old);
        return false;
    }

    return true;
}

void
Msr::BaseBus::reset() {
    for (auto it = _devices.begin(); it != _devices.end(); ++it) {
        it->reset();
    }
}

void
Msr::BaseBus::log_trace_info(const Msr::RegisterBase *reg, Vbus::Access access, uint64 val) {
    ASSERT(access != Vbus::Access::EXEC);
    ASSERT(reg != nullptr);

    if (_last_access != reg) {
        if (_fold && _num_accesses > 1)
            INFO("%s accessed %lu times", _last_access->name(), _num_accesses);
    } else {
        _num_accesses++;
        if (_fold)
            return;
    }

    INFO("%s @%#x %s " FMTx64, reg->name(), reg->id(), (access == Vbus::Access::WRITE ? "W" : "R"), val);
    _num_accesses = 0;
    _last_access = reg;
}

Msr::Err
Msr::BaseBus::access(Vbus::Access access, const VcpuCtx &vcpu_ctx, mword id, uint64 &val) {
    RegisterBase *reg = _devices[id];
    Tsc start_tsc{0ull};

    ASSERT(access != Vbus::Access::EXEC);

    if (reg == nullptr)
        return Msr::Err::NO_DEVICE;

    if (__UNLIKELY__(_stats_enabled)) {
        start_tsc = reg->msr_stats_start(access);
        _msrs_stats.last_access = reg;
        _msrs_stats.last_seen = start_tsc;
        _msrs_stats.total_access++;
    }

    Msr::Err err = reg->access(access, &vcpu_ctx, val);

    if (_trace)
        log_trace_info(reg, access, val);

    if (__UNLIKELY__(_stats_enabled))
        reg->msr_stats_end(start_tsc);

    return err;
}
