/**
 * Copyright (C) 2019-2020 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <msr/msr_id.hpp>
#include <platform/log.hpp>

namespace Msr {

    class Access {
    public:
        static constexpr uint8 INVALID_REG_ACCESS = 0xff;

        Access(uint8 const op0, uint8 const crn, uint8 const op1, uint8 const crm, uint8 const op2, uint8 const gpr_target,
               bool write)
            : _write(write), _target(gpr_target), _second_target(INVALID_REG_ACCESS), _id(build_msr_id(op0, crn, op1, crm, op2)) {
        }
        Access(uint32 id, uint8 const gpr_target, bool write)
            : _write(write), _target(gpr_target), _second_target(INVALID_REG_ACCESS), _id(id) {}

        bool write() const { return _write; }
        uint8 target_reg() const { return _target; }
        uint32 id() const { return _id.id(); }

        // only useful for 32-bit when writing to 64-bit system registers
        bool double_target_reg() const { return _second_target != INVALID_REG_ACCESS; }
        void set_second_target_reg(uint8 t) { _second_target = t; }
        uint8 second_target_reg() const {
            ASSERT(double_target_reg());
            return _second_target;
        }

    private:
        bool _write;
        uint8 _target;
        uint8 _second_target;
        Msr::Id _id;
    };
}
