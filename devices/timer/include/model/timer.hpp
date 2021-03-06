/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/irq_controller.hpp>
#include <model/vcpu_types.hpp>
#include <platform/errno.hpp>

namespace Model {
    class Timer;
};

class Model::Timer {
protected:
    Irq_controller *const _irq_ctlr;
    Vcpu_id const _vcpu;
    uint16 const _irq;

    enum : uint8 { ENABLED_BIT = 0x1, MASKED_BIT = 0x2, STATUS_BIT = 0x4 };

    class Cntv_ctl {
    public:
        Cntv_ctl(uint8 val) : _value(val) {}

        constexpr bool enabled() const { return (_value & ENABLED_BIT) != 0; }
        constexpr bool masked() const { return (_value & MASKED_BIT) != 0; }
        constexpr bool status() const { return (_value & STATUS_BIT) != 0; }
        constexpr bool can_fire() const { return enabled() && !masked(); }
        void set_status() { _value |= STATUS_BIT; }
        void clear_status() { _value &= static_cast<uint8>(~STATUS_BIT); }

        constexpr uint8 get() const { return _value; }
        void set(uint8 val) { _value = val; }

    private:
        uint8 _value;
    };

public:
    Timer(Irq_controller &irq_ctlr, Vcpu_id const vcpu_id, uint16 const irq)
        : _irq_ctlr(&irq_ctlr), _vcpu(vcpu_id), _irq(irq) {}

    bool init_irq(Vcpu_id const vcpu_id, uint16 const pirq, bool hw, bool edge = true) {
        return _irq_ctlr->config_irq(vcpu_id, _irq, hw, pirq, edge);
    }

    bool assert_irq(uint64 const control) {
        Cntv_ctl ctl = Cntv_ctl(uint8(control));

        if (ctl.can_fire()) {
            return _irq_ctlr->assert_ppi(_vcpu, _irq);
        } else {
            return false;
        }
    }

    template<typename T>
    bool schedule_timeout(uint64 const control, uint64 const timeout_absolut, T *host) {
        Cntv_ctl ctl = Cntv_ctl(uint8(control));

        if (!ctl.can_fire())
            return false;
        host->block_timeout(timeout_absolut);
        return true;
    }
};
