/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <msr/msr.hpp>

namespace Msr {
    void flush_on_cache_toggle(const VcpuCtx* vcpu, uint64 new_value);
    class CntpctEl0;
    class CntpTval;
}

class Msr::CntpctEl0 : public RegisterBase {
public:
    explicit CntpctEl0() : RegisterBase("CNTPCT_EL0", CNTPCT_EL0) {}

    virtual Err access(Vbus::Access access, const VcpuCtx* vctx, uint64& value) override {
        if (access != Vbus::READ)
            return Err::ACCESS_ERR;

        value = static_cast<uint64>(clock()) - vctx->regs->tmr_cntvoff();
        return Err::OK;
    }

    virtual void reset(const VcpuCtx*) override {}
};

class Msr::CntpTval : public Register {
private:
    Model::AA64Timer* _ptimer;
    static constexpr uint64 CNTP_TVAL_MASK = 0xffffffffull;

public:
    CntpTval(const char* name, Msr::RegisterId id, Model::AA64Timer& t)
        : Register(name, id, true, 0, CNTP_TVAL_MASK), _ptimer(&t) {}

    virtual Err access(Vbus::Access access, const VcpuCtx* vctx, uint64& value) {
        if (access == Vbus::READ) {
            uint64 cval = _ptimer->get_cval(), curr = static_cast<uint64>(clock()) - vctx->regs->tmr_cntvoff();
            value = (cval - curr) & CNTP_TVAL_MASK;
            return Err::OK;
        } else if (access == Vbus::WRITE) {
            int32 v = static_cast<int32>(value);
            _ptimer->set_cval(static_cast<uint64>(clock()) + static_cast<uint64>(v));
            return Err::OK;
        } else {
            return Err::ACCESS_ERR;
        }
    }
};
