/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
 * see repository root for details.
 */
#pragma once

#include <model/cpu.hpp>
#include <model/physical_timer.hpp>
#include <model/vcpu_types.hpp>
#include <platform/log.hpp>
#include <platform/reg_accessor.hpp>
#include <platform/time.hpp>
#include <vbus/vbus.hpp>

namespace Msr {

    class Id;
    class Access;
    class Register_base;
    class Register;
    class Bus;
    class Id_aa64pfr0;
    class Id_pfr0;
    class Id_pfr1;
    class Ccsidr;
    class Icc_sgi1r_el1;
    class Cntp_ctl_el0;
    class Cntp_cval_el0;
    class Cntp_tval_el0;
    class Cntpct_el0;
    class Set_way_flush_reg;
    class Wtrapped_msr;
    class Sctlr_el1;

    constexpr uint8 CCSIDR_NUM{7};

    static constexpr uint32 build_msr_id(uint8 const op0, uint8 const crn, uint8 const op1,
                                         uint8 const crm, uint8 const op2) {
        return (((uint32(crm) & 0xf) << 3) | ((uint32(crn) & 0xf) << 7)
                | ((uint32(op1) & 0x7) << 10) | ((uint32(op2) & 0x7) << 13)
                | ((uint32(op0) & 0xff) << 16));
    }

    static constexpr uint8 OP0_AARCH32_ONLY_MSR = 0xff;

    /*
     * Future, we can move all register ids declaration here so that we have an unique
     * way to identify them across the code base.
     */
    enum Register_id {
        CTR_A32 = build_msr_id(0b1111, 0b0, 0b0, 0b0, 0b1),
        CTR_A64 = build_msr_id(0b11, 0b0, 0b11, 0b0, 0b1),
        DCISW_A32 = build_msr_id(0b1111, 0b0111, 0b000, 0b0110, 0b010),
        DCISW_A64 = build_msr_id(0b01, 0b0111, 0b000, 0b0110, 0b010),
        DCCSW_A32 = build_msr_id(0b1111, 0b0111, 0b000, 0b1010, 0b010),
        DCCSW_A64 = build_msr_id(0b1111, 0b0111, 0b000, 0b1010, 0b010),
        DCCISW_A32 = build_msr_id(0b1111, 0b0111, 0b000, 0b1110, 0b010),
        DCCISW_A64 = build_msr_id(0b01, 0b0111, 0b000, 0b1110, 0b010),
        MVFR0 = build_msr_id(3, 0, 0, 3, 0),
        MVFR1 = build_msr_id(3, 0, 0, 3, 1),
        MVFR2 = build_msr_id(3, 0, 0, 3, 2),
        CONTEXTIDR_A32 = build_msr_id(0b1111, 0xd, 0, 0, 1),
        CONTEXTIDR_EL1 = build_msr_id(3, 0xd, 0, 3, 1),
        SCTLR_EL1 = Msr::build_msr_id(3, 1, 0, 0, 0),
        TTBR0_EL1 = Msr::build_msr_id(3, 2, 0, 0, 0),
        TTBR1_EL1 = Msr::build_msr_id(3, 2, 0, 0, 1),
        TCR_EL1 = Msr::build_msr_id(3, 2, 0, 0, 2),
        AFSR0_EL1 = Msr::build_msr_id(3, 5, 0, 1, 0),
        AFSR1_EL1 = Msr::build_msr_id(3, 5, 0, 1, 1),
        ESR_EL1 = Msr::build_msr_id(3, 5, 0, 2, 0),
        FAR_EL1 = Msr::build_msr_id(3, 6, 0, 0, 0),
        MAIR_EL1 = Msr::build_msr_id(3, 0xa, 0, 2, 0),
        AMAIR_EL1 = Msr::build_msr_id(3, 0xa, 0, 3, 0),

        /*
         * Below, we define a namespace for registers that do no exist in AA64.
         * We know we are not overlapping with AA64 or AA32 there because op0 (or coproc)
         * is only 4 bits on AA32 and 2 bits in AA64.
         */
        FPSID = build_msr_id(OP0_AARCH32_ONLY_MSR, 0, 0, 3, 0),
        DACR_A32 = build_msr_id(0b1111, 0b0011, 0b000, 0b0000, 0b000),
        DACR = build_msr_id(OP0_AARCH32_ONLY_MSR, 0b0011, 0b000, 0b0000, 0b000),
        MAIR1_A32 = Msr::build_msr_id(3, 0xa, 0, 2, 1),
        IFSR_A32 = build_msr_id(0b1111, 0b0101, 0b000, 0b0000, 0b001),
        IFSR = build_msr_id(OP0_AARCH32_ONLY_MSR, 0b0101, 0b000, 0b0000, 0b001),
        INVALID_ID = build_msr_id(0xff, 0xff, 0xff, 0xff, 0xff),
    };
}

namespace Model {
    class Gic_d;
}

static constexpr uint32
build_msr_id(uint8 const op0, uint8 const crn, uint8 const op1, uint8 const crm, uint8 const op2) {
    return (((uint32(crm) & 0xf) << 1) | ((uint32(crn) & 0xf) << 10) | ((uint32(op1) & 0x7) << 14)
            | ((uint32(op2) & 0x7) << 17) | ((uint32(op0) & 0x3) << 20))
           << 2;
}

class Msr::Id {
private:
    uint32 _id;

public:
    /* align id 8 byte for vbus usage */
    Id(uint8 const op0, uint8 const crn, uint8 const op1, uint8 const crm, uint8 const op2)
        : _id(build_msr_id(op0, crn, op1, crm, op2)) {}
    Id(uint32 id) : _id(id) {}

    uint32 id() const { return _id; }
};

class Msr::Access {
public:
    Access(uint8 const op0, uint8 const crn, uint8 const op1, uint8 const crm, uint8 const op2,
           uint8 const gpr_target, bool write)
        : _write(write), _target(gpr_target), _id(build_msr_id(op0, crn, op1, crm, op2)) {}
    Access(uint32 id, uint8 const gpr_target, bool write)
        : _write(write), _target(gpr_target), _id(id) {}

    bool write() const { return _write; }
    uint8 target_reg() const { return _target; }
    uint32 id() const { return _id.id(); }

private:
    bool _write;
    uint8 _target;
    Msr::Id _id;
};

class Msr::Register_base : public Vbus::Device {
public:
    Register_base(const char* name, Id reg_id) : Vbus::Device::Device(name), _reg_id(reg_id) {}

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vcpu_ctx, mword off, uint8 bytes,
                             uint64& res)
        = 0;

    uint32 id() const { return _reg_id.id(); };

private:
    Id _reg_id;
};

class Msr::Register : public Register_base {
protected:
    uint64 _value;
    uint64 _reset_value;

private:
    uint64 const _write_mask;
    bool const _writable;

public:
    Register(const char* name, Id const reg_id, bool const writable, uint64 const reset_value,
             uint64 const mask = ~0ULL)
        : Register_base(name, reg_id), _value(reset_value), _reset_value(reset_value),
          _write_mask(mask), _writable(writable) {}

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx*, mword, uint8,
                             uint64& value) override {
        if (access == Vbus::WRITE && !_writable)
            return Vbus::Err::ACCESS_ERR;

        if (access == Vbus::WRITE) {
            _value |= value & _write_mask;                    // Set the bits at 1
            _value &= ~(_write_mask & (value ^ _write_mask)); // Set the bits at 0
        } else {
            value = _value;
        }

        return Vbus::Err::OK;
    }

    virtual void reset() override { _value = _reset_value; }
};

class Msr::Set_way_flush_reg : public Msr::Register {
public:
    Set_way_flush_reg(const char* name, Id const reg_id, Vbus::Bus& vbus)
        : Register(name, reg_id, true, 0x0, 0x00000000fffffffeull), _vbus(&vbus) {}
    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vctx, mword off, uint8 bytes,
                             uint64& value) override {
        Vbus::Err ret = Register::access(access, vctx, off, bytes, value);

        if (access == Vbus::Access::WRITE) {
            flush(vctx, (_value >> 1) & 0x7, static_cast<uint32>(_value >> 4));
        }

        return ret;
    }

protected:
    void flush(const Vcpu_ctx*, const uint8, const uint32) const;

    Vbus::Bus* _vbus;
};

class Msr::Id_aa64pfr0 : public Register {
private:
    uint64 _reset_value(uint64 value) const {
        value &= ~(0xfull << 28); /* ras - not implemented */
        value &= ~(0xfull << 32); /* sve - not implemented */
        value &= ~(0xfull << 40); /* MPAM - not implemented */
        value &= ~(0xfull << 44); /* AMU - not implemented */
        return value;
    }

public:
    Id_aa64pfr0(uint64 value)
        : Register("ID_AA64PFR0_EL1", Id(3, 0, 0, 4, 0), false, _reset_value(value)) {}
};

class Msr::Id_pfr0 : public Register {
private:
    uint64 _reset_value(uint32 value) const {
        /* Take the value of the HW for state 0 to 3 (bits[15:0]), the rest is not implemented */
        return value & 0xffffull;
    }

public:
    Id_pfr0(uint32 value)
        : Register("ID_PFR0_EL1", Id(3, 0, 0, 1, 0), false, _reset_value(value)) {}
};

class Msr::Id_pfr1 : public Register {
private:
    uint64 _reset_value(uint32 value) const {
        uint64 ret = value;
        /* Disable the features that require aarch32 el1 to be implemented */
        ret &= ~(0xfull);       /* Disable ProgMod */
        ret &= ~(0xfull << 4);  /* Disable security */
        ret &= ~(0xfull << 12); /* Disable Virt */
        return ret;
    }

public:
    Id_pfr1(uint32 value)
        : Register("ID_PFR1_EL1", Id(3, 0, 0, 1, 1), false, _reset_value(value)) {}
};

class Msr::Ccsidr : public Register_base {
private:
    Register& csselr;
    uint64 const clidr_el1;
    uint64 ccsidr_data_el1[CCSIDR_NUM]{};
    uint64 ccsidr_inst_el1[CCSIDR_NUM]{};

    enum Cache_entry {
        NO_CACHE = 0,
        INSTRUCTION_CACHE_ONLY = 1,
        DATA_CACHE_ONLY = 2,
        SEPARATE_CACHE = 3,
        UNIFIED_CACHE = 4,
        INVALID = 0xffffffff
    };

public:
    Ccsidr(Register& cs, uint64 const clidr, uint64 const* ccsidr)
        : Register_base("CCSIDR_EL1", Id(3, 0, 1, 0, 0)), csselr(cs), clidr_el1(clidr) {
        for (unsigned level = 0; level < CCSIDR_NUM; level++) {
            ccsidr_data_el1[level] = ccsidr[level * 2];
            ccsidr_inst_el1[level] = ccsidr[level * 2 + 1];
        }
    }

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vcpu_ctx, mword, uint8,
                             uint64& value) override {
        if (access == Vbus::WRITE)
            return Vbus::Err::ACCESS_ERR;

        uint64 el = 0;
        if (Vbus::Err::OK != csselr.access(access, vcpu_ctx, 0 /* offset */, 4 /* bytes */, el))
            return Vbus::Err::ACCESS_ERR;

        bool const instr = el & 0x1;
        unsigned const level = (el >> 1) & 0x7;

        if (level > 6)
            return Vbus::Err::ACCESS_ERR;

        uint8 const ce = (clidr_el1 >> (level * 3)) & 0b111;

        if (ce == NO_CACHE || (ce == DATA_CACHE_ONLY && instr)) {
            value = INVALID;
            return Vbus::Err::OK;
        }
        if (ce == INSTRUCTION_CACHE_ONLY || (ce == SEPARATE_CACHE && instr)) {
            value = ccsidr_inst_el1[level];
            return Vbus::Err::OK;
        }

        value = ccsidr_data_el1[level];
        return Vbus::Err::OK;
    }

    virtual void reset() override {}
};

class Msr::Icc_sgi1r_el1 : public Register_base {
private:
    Model::Gic_d* _gic;

public:
    Icc_sgi1r_el1(Model::Gic_d& gic)
        : Register_base("ICC_SGI1R_EL1", Id(3, 0xc, 0x0, 0xb, 5)), _gic(&gic) {}

    virtual Vbus::Err access(Vbus::Access, const Vcpu_ctx*, mword, uint8, uint64&) override;

    virtual void reset() override {}
};

class Msr::Cntp_ctl_el0 : public Register {
private:
    Model::Physical_timer* _ptimer;

public:
    Cntp_ctl_el0(Model::Physical_timer& t)
        : Register("CNTP_CTL_EL0", Id(3, 0xe, 3, 2, 1), true, 0, 0b11), _ptimer(&t) {}

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vcpu_ctx, mword addr, uint8 size,
                             uint64& value) {
        _value = _ptimer->get_ctl();

        Vbus::Err err = Register::access(access, vcpu_ctx, addr, size, value);
        if (err == Vbus::OK && access == Vbus::WRITE) {
            _ptimer->set_ctl(static_cast<uint8>(_value));
        }

        return err;
    }
};

class Msr::Cntp_cval_el0 : public Register {
private:
    Model::Physical_timer* _ptimer;

public:
    Cntp_cval_el0(Model::Physical_timer& t)
        : Register("CNTP_CVAL_EL0", Id(3, 0xe, 3, 2, 2), true, 0), _ptimer(&t) {}

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vcpu_ctx, mword addr, uint8 size,
                             uint64& value) {
        _value = _ptimer->get_cval();

        Vbus::Err err = Register::access(access, vcpu_ctx, addr, size, value);
        if (err == Vbus::OK && access == Vbus::WRITE) {
            _ptimer->set_cval(_value);
        }

        return err;
    }
};

class Msr::Cntpct_el0 : public Register_base {
public:
    Cntpct_el0() : Register_base("CNTPCT_EL0", Id(3, 0xe, 3, 0, 1)) {}

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vctx, mword, uint8,
                             uint64& value) override {
        if (access != Vbus::READ)
            return Vbus::Err::ACCESS_ERR;

        Reg_accessor regs(*vctx->ctx, vctx->mtd_in);

        value = static_cast<uint64>(clock()) - regs.tmr_cntvoff();
        return Vbus::Err::OK;
    }

    virtual void reset() override {}
};

class Msr::Cntp_tval_el0 : public Register {
private:
    Model::Physical_timer* _ptimer;
    static constexpr uint64 CNTP_TVAL_MASK = 0xffffffffull;

public:
    Cntp_tval_el0(Model::Physical_timer& t)
        : Register("CNTP_TVAL_EL0", Id(3, 0xe, 3, 2, 0), true, 0, CNTP_TVAL_MASK), _ptimer(&t) {}

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vctx, mword, uint8,
                             uint64& value) {
        if (access == Vbus::READ) {
            Reg_accessor regs(*vctx->ctx, vctx->mtd_in);
            uint64 cval = _ptimer->get_cval(),
                   curr = static_cast<uint64>(clock()) - regs.tmr_cntvoff();
            value = (cval - curr) & CNTP_TVAL_MASK;
            return Vbus::Err::OK;
        } else if (access == Vbus::WRITE) {
            int32 v = static_cast<int32>(value);
            _ptimer->set_cval(static_cast<uint64>(clock()) + static_cast<uint64>(v));
            return Vbus::Err::OK;
        } else {
            return Vbus::Err::ACCESS_ERR;
        }
    }
};

class Msr::Wtrapped_msr : public Msr::Register_base {
public:
    using Msr::Register_base::Register_base;

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx*, mword, uint8, uint64&) override {
        ASSERT(access == Vbus::Access::WRITE); // We only trap writes at the moment
        return Vbus::Err::UPDATE_REGISTER;     // Tell the VCPU to update the relevant physical
                                               // register
    }
    virtual void reset() override {}
};

class Msr::Sctlr_el1 : public Msr::Register_base {
public:
    Sctlr_el1(const char* name, Msr::Id reg_id, Vbus::Bus& vbus)
        : Msr::Register_base(name, reg_id), _vbus(&vbus) {}

    virtual Vbus::Err access(Vbus::Access access, const Vcpu_ctx* vcpu, mword, uint8,
                             uint64& res) override;
    virtual void reset() override {}

private:
    Vbus::Bus* _vbus;
};

/*
 * This is the bus that will handle all reads and writes
 * to system registers.
 */
class Msr::Bus : public Vbus::Bus {
    using Vbus::Bus::Bus;

private:
    bool setup_aarch64_features(uint64 id_aa64pfr0_el1, uint64 id_aa64pfr1_el1,
                                uint64 id_aa64isar0_el1, uint64 id_aa64isar1_el1,
                                uint64 id_aa64zfr0_el1);
    bool setup_aarch64_caching_info(Vbus::Bus& vbus, uint64 ctr_el0, uint64 clidr_el1,
                                    uint64 ccsidr_el1[CCSIDR_NUM * 2]);
    bool setup_aarch64_memory_model(uint64 id_aa64mmfr0_el1, uint64 id_aa64mmfr1_el1,
                                    uint64 id_aa64mmfr2_el1);
    bool setup_aarch64_debug(uint64 id_aa64dfr0_el1, uint64 id_aa64dfr1_el1);
    bool setup_aarch64_auxiliary();

    bool setup_aarch32_features(uint32 id_pfr0_el1, uint32 id_pfr1_el1, uint32 id_pfr2_el1,
                                uint32 id_isar0_el1, uint32 id_isar1_el1, uint32 id_isar2_el1,
                                uint32 id_isar3_el1, uint32 id_isar4_el1, uint32 id_isar5_el1,
                                uint32 id_isar6_el1);
    bool setup_aarch32_memory_model(uint32 id_mmfr0_el1, uint32 id_mmfr1_el1, uint32 id_mmfr2_el1,
                                    uint32 id_mmfr3_el1, uint32 id_mmfr4_el1, uint32 id_mmfr5_el1);
    bool setup_aarch32_media_vfp(uint32 mvfr0_el1, uint32 mvfr1_el1, uint32 mvfr2_el1,
                                 uint64 midr_el1);
    bool setup_aarch32_debug(uint64 id_aa64dfr0_el1, uint32 id_dfr0_el1);

    virtual bool setup_page_table_regs(Vbus::Bus&);
    bool setup_tvm(Vbus::Bus&);

protected:
    bool register_system_reg(Register_base* reg) {
        ASSERT(reg != nullptr);
        return register_device(reg, reg->id(), sizeof(uint64));
    }

public:
    bool setup_aarch64_physical_timer(Model::Physical_timer& ptimer);

    struct Platform_info {
        // AArch64 registers
        uint64 id_aa64pfr0_el1;
        uint64 id_aa64pfr1_el1;
        uint64 id_aa64dfr0_el1;
        uint64 id_aa64dfr1_el1;
        uint64 id_aa64isar0_el1;
        uint64 id_aa64isar1_el1;
        uint64 id_aa64mmfr0_el1;
        uint64 id_aa64mmfr1_el1;
        uint64 id_aa64mmfr2_el1;
        uint64 id_aa64zfr0_el1;
        uint64 midr_el1;

        // AArch32 registers
        uint32 id_pfr0_el1;
        uint32 id_pfr1_el1;
        uint32 id_pfr2_el1;
        uint32 id_dfr0_el1;
        uint32 id_dfr1_el1;
        uint32 id_isar0_el1;
        uint32 id_isar1_el1;
        uint32 id_isar2_el1;
        uint32 id_isar3_el1;
        uint32 id_isar4_el1;
        uint32 id_isar5_el1;
        uint32 id_isar6_el1;
        uint32 id_mmfr0_el1;
        uint32 id_mmfr1_el1;
        uint32 id_mmfr2_el1;
        uint32 id_mmfr3_el1;
        uint32 id_mmfr4_el1;
        uint32 id_mmfr5_el1;
        uint32 mvfr0_el1;
        uint32 mvfr1_el1;
        uint32 mvfr2_el1;

        // Cache topology
        uint64 ctr_el0;
        uint64 clidr_el1;
        uint64 ccsidr_el1[CCSIDR_NUM * 2];
    };

    bool setup_arch_msr(Platform_info& info, Vbus::Bus&, Model::Gic_d&);
};
