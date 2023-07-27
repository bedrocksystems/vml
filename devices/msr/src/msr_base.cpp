/**
 * Copyright (C) 2023 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <msr/msr_base.hpp>

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
Msr::BaseBus::reset(const VcpuCtx &vcpu_ctx) {
    for (auto it = _devices.begin(); it != _devices.end(); ++it) {
        it->reset(&vcpu_ctx);
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

    ASSERT(access != Vbus::Access::EXEC);

    if (reg == nullptr)
        return Msr::Err::NO_DEVICE;
    if (_trace)
        log_trace_info(reg, access, val);

    return reg->access(access, &vcpu_ctx, val);
}
