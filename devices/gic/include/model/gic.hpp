/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <debug_switches.hpp>
#include <model/cpu_affinity.hpp>
#include <model/irq_controller.hpp>
#include <model/vcpu_types.hpp>
#include <platform/atomic.hpp>
#include <platform/bits.hpp>
#include <platform/bitset.hpp>
#include <platform/compiler.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <vbus/vbus.hpp>

namespace Model {
    class Cpu_irq_interface;
    class GicD;
    class GicR;

    enum { ACCESS_SIZE_32 = 4 };

    static constexpr uint32 SPECIAL_INTID_NONE = 1023;
    static constexpr uint8 PRIORITY_ANY = 0xff;
    static constexpr uint8 GICV2_MAX_CPUS = 8;
    // Must a multiple of 32, bigger than 32 (to fit PPIs and SGIs)
    static constexpr uint8 GICD_MIN_LINES = 64;
}

class Model::GicD : public Model::Irq_controller {
    friend GicR;

private:
    struct Banked;

    class IrqInjectionInfo;

    class IrqTarget {
    public:
        /* The CPU set format is only supported in GICv2 mode. A consequence of that
         * is that the mask will never have more than 8 bits sets because GICv2 will
         * no handle more CPUs than that.
         */
        enum Format { CPU_ID = 0u << 31, CPU_SET = 1u << 31 };

        static constexpr uint32 FORMAT_MASK = 1u << 31;
        static constexpr uint32 TARGET_DATA_MASK = ~FORMAT_MASK;
        static constexpr uint32 INVALID_TARGET = ~0x0u;

        IrqTarget() : _tgt(INVALID_TARGET) {}
        IrqTarget(Format f, uint64 target) : _tgt((f & FORMAT_MASK) | (static_cast<uint32>(target) & TARGET_DATA_MASK)) {}
        explicit IrqTarget(uint32 raw) : _tgt(raw) {}

        bool is_valid() const { return _tgt != INVALID_TARGET; }
        uint32 raw() const { return _tgt; }
        uint32 target() const { return _tgt & TARGET_DATA_MASK; }
        bool is_targeting_a_set() const { return (_tgt & CPU_SET) != 0u; }
        bool is_cpu_targeted(Vcpu_id id) const {
            if (!is_targeting_a_set())
                return target() == id;
            else {
                ASSERT(id < 8);
                return (target() & (1u << id)) != 0u;
            }
        }

        void add_target_to_set(Vcpu_id id) {
            ASSERT(is_targeting_a_set());
            ASSERT(id < 8);
            _tgt |= 1u << id;
        }

    private:
        uint32 _tgt;
    };

    class IrqInjectionInfoUpdate {
    public:
        friend class IrqInjectionInfo;

        explicit IrqInjectionInfoUpdate(uint64 val = 0) : _info(val) {}

        static constexpr uint8 PENDING_SHIFT = 32;
        static constexpr uint64 PENDING_BIT = 1ull << PENDING_SHIFT;
        static constexpr uint64 PENDING_FIELD = 0xffull << PENDING_SHIFT;
        static constexpr uint8 INJECTED_SHIFT = 40;
        static constexpr uint64 INJECTED_BIT = 1ull << INJECTED_SHIFT;
        static constexpr uint64 INJECTED_FIELD = 0xffull << INJECTED_SHIFT;

        bool pending() const { return (_info & PENDING_FIELD) != 0; }
        bool in_injection() const { return (_info & INJECTED_FIELD) != 0; }
        bool is_targeting_cpu(Vcpu_id id) const {
            IrqTarget tgt(static_cast<uint32>(_info));
            return tgt.is_cpu_targeted(id);
        }

        void set_target_cpu(const IrqTarget &tgt) { _info = (_info & 0xffffffff00000000ull) | tgt.raw(); }

        /*
         * The following functions are expected to be called with sender_id = 0.
         * The only exception is SGIs that are banked by sender.
         */
        bool is_injected(uint8 sender_id = 0) const {
            ASSERT(sender_id != NO_INJECTION);
            ASSERT(sender_id < Model::GICV2_MAX_CPUS);
            return (_info & (INJECTED_BIT << sender_id)) != 0u;
        }
        void set_injected(uint8 sender_id = 0) {
            ASSERT(sender_id < Model::GICV2_MAX_CPUS);
            _info |= (INJECTED_BIT << sender_id);
        }
        void unset_injected(uint8 sender_id = 0) {
            ASSERT(sender_id < Model::GICV2_MAX_CPUS);
            _info &= ~(INJECTED_BIT << sender_id);
        }
        void set_pending(uint8 sender_id = 0) {
            ASSERT(sender_id < Model::GICV2_MAX_CPUS);
            _info |= (PENDING_BIT << sender_id);
        }
        void unset_pending(uint8 sender_id = 0) {
            ASSERT(sender_id < Model::GICV2_MAX_CPUS);
            _info &= ~(PENDING_BIT << sender_id);
        }

        /*
         * The function below is only relevant for SGIs with affinity routing disabled.
         * In that configuration, SGIs are banked by senders and up to 8 CPUs are supported.
         */
        uint8 get_pending_sender_id() const {
            int sender_bit_set = ffs(static_cast<unsigned int>(((_info & PENDING_FIELD) >> PENDING_SHIFT)));
            ASSERT(sender_bit_set > 0);
            ASSERT(sender_bit_set <= Model::GICV2_MAX_CPUS);

            return static_cast<uint8>(sender_bit_set - 1);
        }

        static constexpr uint8 NO_INJECTION = 0xff;

        uint8 get_injected_sender_id() const {
            int sender_bit_set = ffs(static_cast<unsigned int>(((_info & INJECTED_FIELD) >> INJECTED_SHIFT)));
            if (sender_bit_set == 0)
                return NO_INJECTION;

            ASSERT(sender_bit_set <= Model::GICV2_MAX_CPUS);

            return static_cast<uint8>(sender_bit_set - 1);
        }

    private:
        uint64 _info;
    };

    class IrqInjectionInfo {
    public:
        explicit IrqInjectionInfo(uint64 val) : _info(val) {}

        IrqInjectionInfoUpdate read() const { return IrqInjectionInfoUpdate(_info); }
        bool cas(IrqInjectionInfoUpdate &expected, IrqInjectionInfoUpdate &desired) {
            return _info.cas(expected._info, desired._info);
        }

        void set(IrqInjectionInfoUpdate &new_val) { _info = new_val._info; }

    private:
        /*
         * The structure of routing_info is as such:
         * Bit 0->31: CPU that owns the interrupt (after routing)
         * Bit 32->39: Pending bits (Banked by Sender for SGIs with no affinity routing - 8 CPUs)
         * Bit 40->47: Injected bits (Banked by Sender for SGIs with no affinity routing - 8 CPUs)
         */
        atomic<uint64> _info;
    };

    class Irq {
        uint16 _id{0};
        uint16 _pintid{0};
        uint8 _prio{0};
        uint8 _target{1};

        /*
         * We are maintaining info in three places when it comes to the pending bit.
         * 1 - We remember if the device line is asserted or not
         * 2 - We remember if the guest wrote "1" in the pending bit of that IRQ
         * 3 - We maintain pending info in "injection_info". injection_info is not something
         * that is directly visible to a guest (although he may read info derived from this value
         * and the two previous). Its main job is for to keep track of when the interrupt should be
         * injected inside the guest.
         */
        bool _line_asserted{false}; // Asserted by the HW/vDevice
        bool _sw_asserted{false};   // Asserted in software by the guest

        /*
         * HW edge is the underlying virtual HW configuration that cannot be changed
         * by the guest. The guest can change _sw_edge for SPIs but not for other types
         * of interrupts. Note that even for SPIs, a read from the GIC device always give
         * the HW view and not the configured one.
         */
        bool _hw_edge{true};
        bool _sw_edge{true};
        bool _enable{false};
        bool _group1{false};
        bool _hw{false};
        bool _active{false};

    public:
        IrqInjectionInfo injection_info{0};

        struct {
            uint64 value{0};
            constexpr uint8 aff0() const { return value & 0xff; }
            constexpr uint8 aff1() const { return (value >> 8) & 0xff; }
            constexpr uint8 aff2() const { return (value >> 16) & 0xff; }
            constexpr uint8 aff3() const { return (value >> 32) & 0x1f; }
            constexpr bool any() const { return ((value >> 31) & 0x1) != 0u; }
        } routing;

        void enable(bool mmio_one = true) {
            if (mmio_one)
                _enable = true;
        }
        void disable(bool mmio_one = true) {
            if (mmio_one)
                _enable = false;
        }

        bool group0() const { return !_group1; }
        bool group1() const { return _group1; }
        void set_group1(bool group1) { _group1 = group1; }

        uint8 prio() const { return _prio; }
        void prio(uint8 const p) { _prio = p; }

        void set_id(uint16 id) { _id = id; }
        uint16 id() const { return _id; }
        bool hw() const { return _hw; }
        uint16 hw_int_id() const { return _pintid; }
        bool hw_edge() const { return _hw_edge; }
        bool sw_edge() const { return _sw_edge; }
        void configure_hw(bool hw, uint16 pintid = 0, bool edge = false) {
            _pintid = pintid;
            _hw_edge = edge;
            _sw_edge = edge;
            _hw = hw;
        }
        uint8 edge_encoded() const { return sw_edge() ? 0b10 : 0; }
        void set_encoded_edge(uint8 encoded_edge) { _sw_edge = ((encoded_edge & 0x2) != 0); }

        uint8 target() const { return _target; }
        void target(uint8 const t) {
            if (__UNLIKELY__(Debug::current_level > Debug::CONDENSED))
                INFO("GOS requested IRQ %u to be routed to VCPU(s) mask 0x%x", _id, t);
            _target = t;
        }

        bool enabled() const { return _enable; }
        bool active() const { return _active; }
        void activate(bool mmio_one = true) {
            if (mmio_one)
                _active = true;
        }
        void deactivate(bool mmio_one = true) {
            if (mmio_one)
                _active = false;
        }

        bool pending() const {
            /*
             * An interrupt is pending if:
             * - the device line is asserted and the IRQ is configured as level.
             * - the guest wrote to the pending bit, setting it to 1.
             * - The injection info is pending meaning that the IRQ was pending previously
             * asserted due to an edge IRQ firing.
             */
            return (!sw_edge() && _line_asserted) || _sw_asserted || injection_info.read().pending();
        }

        void reset(uint8 t) {
            _sw_edge = _hw_edge;
            target(t);
            prio(0);
            deactivate();
            disable();
            set_group1(false);

            if (!_hw) {
                IrqInjectionInfoUpdate update(0);
                injection_info.set(update);
            }

            routing.value = 0;
        }

        void assert_line() { _line_asserted = true; }
        void deassert_line() { _line_asserted = false; }
        void assert_sw() { _sw_asserted = true; }
        void deassert_sw() { _sw_asserted = false; }
        bool sw_asserted() const { return _sw_asserted; }

        // stats purposes
        atomic<uint64> num_asserted{0};
        atomic<uint64> num_acked{0};
    };

public:
    enum IrqState { INACTIVE = 0, PENDING = 1, ACTIVE = 2, ACTIVE_PENDING = 3 };

    class Lr {
    private:
        uint64 _lr;

        static constexpr uint64 PIRQ_ID_MASK = 0x3ffull;
        static constexpr uint8 PIRQ_ID_SHIFT = 32;
        static constexpr uint64 SENDER_MASK = 0x7ull;
        static constexpr uint8 SENDER_SHIFT = 32;
        static constexpr uint8 PRIO_SHIFT = 48;
        static constexpr uint64 PRIO_MASK = 0xffull;
        static constexpr uint8 GROUP_BIT_SHIFT = 60;
        static constexpr uint8 HW_BIT_SHIFT = 61;
        static constexpr uint8 STATE_SHIFT = 62;
        static constexpr uint64 VIRQ_ID_MASK = 0xffffffffull;
        static constexpr uint64 STATE_MASK = 0x3ull;

    public:
        explicit Lr(uint64 const lr) : _lr(lr) {}
        Lr(IrqState const state, Irq &irq, uint32 const vintid, uint8 sender = 0) : _lr(0) {
            _lr |= static_cast<uint64>(state) << STATE_SHIFT;
            _lr |= (irq.hw() ? 1ull : 0ull) << HW_BIT_SHIFT;
            _lr |= (irq.group1() ? 1ULL : 0ull) << GROUP_BIT_SHIFT;

            _lr |= static_cast<uint64>(irq.prio()) << PRIO_SHIFT; /* 8 bit - 48-55 */

            if (irq.hw()) {
                _lr |= static_cast<uint64>(irq.hw_int_id() & PIRQ_ID_MASK) << PIRQ_ID_SHIFT; /* 10bit - 32-41 */
            } else if (vintid < MAX_SGI) {
                /*
                 * This can be surprising to read - the data has to go in
                 * that field because the NOVA API uses the GICv3 interface
                 * and converts it back to v2. Also, the GIC model guarantees
                 * that this field will always be zero if we are emulating a
                 * GICv3.
                 */
                _lr |= static_cast<uint64>(sender & SENDER_MASK) << SENDER_SHIFT;
            }
            _lr |= static_cast<uint64>(vintid); /* low 32bit */
        }

        IrqState state() const { return static_cast<IrqState>((_lr >> STATE_SHIFT) & STATE_MASK); }
        void set_state(IrqState st) { _lr = (_lr & ~(STATE_MASK << STATE_SHIFT)) | (static_cast<uint64>(st) << STATE_SHIFT); }
        void activate() { set_state(IrqState::ACTIVE); }
        void deactivate() { set_state(IrqState::INACTIVE); }

        bool hw() const { return (_lr & (1ull << HW_BIT_SHIFT)) != 0u; }
        uint32 pintid() const { return (_lr >> PIRQ_ID_SHIFT) & PIRQ_ID_MASK; }
        uint32 vintid() const { return _lr & VIRQ_ID_MASK; }
        uint8 senderid() const { return hw() ? 0 : static_cast<uint8>((_lr >> SENDER_SHIFT) & SENDER_MASK); }
        uint64 value() const { return _lr; }
        uint8 priority() const { return (_lr >> PRIO_SHIFT) & PRIO_MASK; }
    };

private:
    struct Banked {
        Irq sgi[MAX_SGI];
        Irq ppi[MAX_PPI];

        AtomicBitset<MAX_IRQ> pending_irqs;
        AtomicBitset<MAX_IRQ> in_injection_irqs;
        Cpu_irq_interface *notify{nullptr};

        Banked() {
            for (uint8 i = 0; i < MAX_SGI; i++)
                sgi[i].set_id(i);
            for (uint8 i = 0; i < MAX_PPI; i++)
                ppi[i].set_id(static_cast<uint16>(i + MAX_SGI));
        }
    };

    IRQCtlrVersion const _version;
    uint16 const _num_vcpus;
    uint16 const _configured_irqs;

    struct {
        uint32 value{0};
        constexpr bool group0_enabled() const { return (value & 0x1) != 0u; }
        constexpr bool group1_enabled() const { return (value & 0x2) != 0u; }
        constexpr bool affinity_routing() const { return (value & 0x10) != 0u; }
    } _ctlr;

    Banked *_local{nullptr};
    Irq *_spi{nullptr};

    // simple way to round-robin IRQs when GICv3 is enabled
    atomic<uint16> _vcpu_global_hint{0};

    Irq &irq_object(Banked &cpu, uint64 const id) {
        if (id < MAX_SGI)
            return cpu.sgi[id];
        if (id < MAX_SGI + MAX_PPI)
            return cpu.ppi[id - MAX_SGI];
        else
            return _spi[id - MAX_SGI - MAX_PPI];
    }

    Irq const &irq_object(Banked const &cpu, uint64 const id) const {
        if (id < MAX_SGI)
            return cpu.sgi[id];
        if (id < MAX_SGI + MAX_PPI)
            return cpu.ppi[id - MAX_SGI];
        else
            return _spi[id - MAX_SGI - MAX_PPI];
    }

    enum AccessType {
        SGI,
        PPI,
        PRIVATE_ONLY,
        SPI,
        ALL,
    };

    struct IrqMmioAccess {
        uint64 base_abs;        // Base of the access (GIC register)
        uint16 irq_base;        // First IRQ that can be targeted (SGI or PPI of SPI offset)
        uint16 irq_max;         // Max IRQ that can be targeted
        uint64 offset;          // offset from the base_abs register
        uint8 bytes;            // size of the access
        uint8 irq_per_bytes;    // number of IRQs covered by one bytes
        uint16 configured_irqs; // Maximum number of lines/irqs exposed by the GIC

        // First IRQ that will be modified by this access
        uint16 first_irq_accessed() const { return static_cast<uint16>(((offset - base_abs) * irq_per_bytes) + irq_base); }

        // Number of IRQ(s) concerned by this access
        uint16 num_irqs() const {
            if (first_irq_accessed() >= irq_max)
                return 0;

            uint16 n = static_cast<uint16>(bytes * irq_per_bytes);
            uint16 abs = first_irq_accessed() + n;
            uint16 overflow = 0;

            if (abs > irq_max)
                overflow = abs - irq_max;

            ASSERT(n >= overflow);
            return n - overflow;
        }

        /*
         * One subtle thing to note: an access will be considered valid if it acceses an IRQ
         * below 1024 (max from the spec). However, it will be ignored (truncated in the configure
         * call and num_irqs) if it goes below the configured max IRQ. This is the behavior of a
         * bare-metal GIC.
         */
        bool is_valid() const {
            if (first_irq_accessed() - irq_base + num_irqs() > MAX_SPI + MAX_PPI + MAX_SGI)
                return false;
            if (first_irq_accessed() + num_irqs() > MAX_SPI + MAX_PPI + MAX_SGI)
                return false;
            return true;
        }

        void configure_access(AccessType type) {
            switch (type) {
            case ALL:
                irq_base = 0;
                irq_max = configured_irqs;
                break;
            case SGI:
                irq_base = SGI_BASE;
                irq_max = SGI_BASE + MAX_SGI;
                break;
            case PPI:
                irq_base = PPI_BASE;
                irq_max = PPI_BASE + MAX_PPI;
                break;
            case SPI:
                irq_base = SPI_BASE;
                irq_max = static_cast<uint16>(SPI_BASE + configured_irqs - MAX_SGI - MAX_PPI);
                break;
            case PRIVATE_ONLY:
                irq_base = 0;
                irq_max = MAX_SGI + MAX_PPI;
                break;
            }
        }
    };

    static uint64 irq_per_bytes_to_mask(uint8 irq_per_bytes) {
        switch (irq_per_bytes) {
        case (8):
            return 0x1ull;
        case (4):
            return 0x3ull;
        case (1):
            return 0xffull;
        default:
            ABORT_WITH("This shouldn't be reached");
            return 0;
        }
    }

    static uint8 irq_per_bytes_to_bits(uint8 irq_per_bytes) {
        switch (irq_per_bytes) {
        case (8):
            return 1;
        case (4):
            return 2;
        case (1):
            return 8;
        default:
            ABORT_WITH("This shouldn't be reached");
            return 0;
        }
    }

    template<typename T, void (Irq::*IRQ_FUN)(T)>
    bool write(Banked &cpu, const IrqMmioAccess &acc, uint64 const value) {
        if (!acc.is_valid())
            return false;

        for (unsigned i = 0; i < acc.num_irqs(); i++) {
            uint64 const pos = acc.first_irq_accessed() + i;
            Irq &irq = irq_object(cpu, pos);
            T const val = static_cast<T>((value >> (i * irq_per_bytes_to_bits(acc.irq_per_bytes)))
                                         & irq_per_bytes_to_mask(acc.irq_per_bytes));
            (irq.*IRQ_FUN)(val);
        }
        return true;
    }

    bool change_target(Banked &cpu, const IrqMmioAccess &acc, uint64 const value) {
        if (!acc.is_valid())
            return false;

        for (unsigned i = 0; i < acc.num_irqs(); i++) {
            uint64 const pos = acc.first_irq_accessed() + i;
            Irq &irq = irq_object(cpu, pos);
            uint8 const val = static_cast<uint8>((value >> (i * irq_per_bytes_to_bits(acc.irq_per_bytes)))
                                                 & irq_per_bytes_to_mask(acc.irq_per_bytes));

            irq.target(val);

            if (irq.pending())
                redirect_spi(irq, ++_vcpu_global_hint);
        }
        return true;
    }

    template<bool (GicD::*GIC_FUN)(Vcpu_id, Vcpu_id, Irq &)>
    bool mmio_assert_sgi(Vcpu_id vcpu_id, const IrqMmioAccess &acc, uint64 const value) {
        if (!acc.is_valid())
            return false;

        for (unsigned i = 0; i < acc.num_irqs(); i++) {
            uint64 const pos = acc.first_irq_accessed() + i;
            Irq &irq = irq_object(_local[vcpu_id], pos);

            uint8 sender_bitfield = static_cast<uint8>(value >> (i * Model::GICV2_MAX_CPUS));

            for (uint8 j = 0; j < Model::GICV2_MAX_CPUS; ++j) {
                if ((sender_bitfield >> j) & 0x1) {
                    (*this.*GIC_FUN)(vcpu_id, j, irq);
                }
            }
        }
        return true;
    }

    template<bool (GicD::*GIC_FUN)(Vcpu_id, Irq &)>
    bool mmio_assert(Vcpu_id cpu_id, const IrqMmioAccess &acc, uint64 const value) {
        if (!acc.is_valid())
            return false;

        for (unsigned i = 0; i < acc.num_irqs(); i++) {
            uint64 const pos = acc.first_irq_accessed() + i;
            Irq &irq = irq_object(_local[cpu_id], pos);

            bool const set = (value >> i) & 0x1;
            if (!set)
                continue;

            (*this.*GIC_FUN)(cpu_id, irq);
        }
        return true;
    }

    template<typename T, T (Irq::*IRQ_FUN)() const>
    bool read(Banked const &cpu, const IrqMmioAccess &acc, uint64 &value) const {
        if (!acc.is_valid())
            return false;

        value = 0;
        for (unsigned i = 0; i < acc.num_irqs(); i++) {
            uint64 const pos = acc.first_irq_accessed() + i;
            Irq const &irq = irq_object(cpu, pos);

            value |= static_cast<uint64>((irq.*IRQ_FUN)()) << (i * irq_per_bytes_to_bits(acc.irq_per_bytes));
        }

        return true;
    }

    struct RegAccess {
        uint64 offset;
        uint32 base_reg;
        uint32 base_max;
        uint8 bytes;
    };

    template<typename T>
    bool write_register(const RegAccess &acc, uint64 const value, T &result, T fixed_clear = 0, T fixed_set = 0) {
        unsigned constexpr TSIZE = sizeof(T);
        if (!acc.bytes || (acc.bytes > TSIZE) || (acc.offset + acc.bytes > acc.base_max + 1))
            return false;

        uint64 const base = acc.offset - acc.base_reg;
        uint64 const mask = (acc.bytes >= TSIZE) ? (static_cast<T>(0) - 1) : ((static_cast<T>(1) << (acc.bytes * 8)) - 1);

        result &= (acc.bytes >= TSIZE) ? static_cast<T>(0) : ~(static_cast<T>(mask) << (base * 8));
        result |= static_cast<T>(value & mask) << (base * 8);
        result &= ~fixed_clear;
        result |= fixed_set;
        return true;
    }

    bool write_ctlr(uint64 offset, uint8 bytes, uint64 value);
    bool write_irouter(Banked &cpu, uint64 offset, uint8 bytes, uint64 value);
    bool write_sgir(Vcpu_id cpu_id, uint64 value);
    static bool read_register(uint64 offset, uint32 base_reg, uint32 base_max, uint8 bytes, uint64 value, uint64 &result);
    bool read_pending(Banked &cpu, IrqMmioAccess &acc, uint32 base_offset, uint64 &value) const;

    void send_sgi(Vcpu_id from, Vcpu_id target, uint32 sgi_id);

    bool mmio_write(Vcpu_id, uint64 offset, uint8 bytes, uint64 value);

    bool mmio_write_32_or_less(Vcpu_id cpu_id, IrqMmioAccess &acc, uint64 value);
    bool mmio_read_32_or_less(Vcpu_id cpu_id, IrqMmioAccess &acc, uint64 &value) const;
    bool mmio_read(Vcpu_id, uint64 offset, uint8 bytes, uint64 &value) const;

    bool assert_sgi(Vcpu_id, Vcpu_id, Irq &irq);
    bool assert_pi(Vcpu_id vcpu_id, Irq &irq);
    bool assert_pi_sw(Vcpu_id vcpu_id, Irq &irq);
    bool deassert_pi(Vcpu_id vcpu_id, Irq &irq);
    bool deassert_pi_sw(Vcpu_id vcpu_id, Irq &irq);
    bool deassert_sgi(Vcpu_id, Vcpu_id, Irq &irq);
    void deassert_line(Vcpu_id cpu_id, uint32 irq_id);

    bool notify_target(Irq &irq, const IrqTarget &target);
    IrqTarget route_spi(Model::GicD::Irq &irq, Vcpu_id vcpu_hint_start);
    IrqTarget route_spi_no_affinity(Model::GicD::Irq &irq);
    bool redirect_spi(Irq &irq, Vcpu_id vcpu_hint_start);
    Irq *highest_irq(Vcpu_id cpu_id, bool redirect_irq);
    bool vcpu_can_receive_irq(const Local_Irq_controller *gic_r) const {
        return !_ctlr.affinity_routing() || gic_r->can_receive_irq();
    }
    void reset_status_bitfields_on_vcpu(uint16 vcpu_idx);
    uint64 get_typer() const {
        uint64 itl = configured_irqs() == MAX_IRQ ? 31ull : (configured_irqs() / 32) - 1;
        return itl | (static_cast<uint64>(_num_vcpus - 1) << 5) /* CPU count */
               | (9ULL << 19)                                   /* id bits */
               | (1ULL << 24);                                  /* Aff3 supported */
    }

    void update_inj_status_inactive(Vcpu_id cpu_id, uint32 irq_id);
    void update_inj_status_active_or_pending(Vcpu_id cpu_id, IrqState state, uint32 irq_id, bool in_injection);

    static uint16 compute_irq_lines(uint16 desired) {
        return static_cast<uint16>(std::min<uint64>(MAX_IRQ, align_up(desired, 32)));
    }

public:
    GicD(IRQCtlrVersion const version, uint16 num_vcpus, uint16 conf_irqs = MAX_IRQ)
        : Irq_controller("GICD"), _version(version), _num_vcpus(num_vcpus), _configured_irqs(compute_irq_lines(conf_irqs)) {}

    ~GicD() override {
        delete[] _local;
        delete[] _spi;
    }

    uint16 configured_irqs() const { return _configured_irqs; }
    uint16 configured_spis() const { return static_cast<uint16>(_configured_irqs - MAX_PPI - MAX_SGI); }

    bool init() {
        if (_version == GIC_V2 && _num_vcpus > GICV2_MAX_CPUS)
            return false;

        if (configured_irqs() <= SPI_BASE) {
            ERROR("GICD configured with only %u IRQs. Not enough room for SPIs", configured_irqs());
            return false;
        }

        INFO("GICD configured with %u IRQ lines total", configured_irqs());

        _local = new (nothrow) Banked[_num_vcpus];
        _spi = new (nothrow) Irq[configured_spis()];
        if (_local == nullptr or _spi == nullptr)
            return false;
        for (uint16 i = 0; i < configured_spis(); i++)
            _spi[i].set_id(static_cast<uint16>(MAX_PPI + MAX_SGI + i));

        reset(nullptr); // vctx not used for now

        return true;
    }

    Vbus::Err access(Vbus::Access, const VcpuCtx *, Vbus::Space, mword, uint8, uint64 &) override;
    void reset(const VcpuCtx *) override;

    bool config_irq(Vcpu_id, uint32 irq_id, bool hw, uint16 pintid, bool edge) override;
    bool config_spi(uint32 vintid, bool hw, uint16 pintid, bool edge) override;
    bool assert_ppi(Vcpu_id, uint32) override;
    bool assert_global_line(uint32) override;
    void deassert_line_ppi(Vcpu_id, uint32) override;
    void deassert_global_line(uint32) override;
    void enable_cpu(Cpu_irq_interface *, Vcpu_id) override;
    void disable_cpu(Vcpu_id) override;
    void assert_msi(uint64, uint32, IrqAssertionRecord *) override { ABORT_WITH("GICD: no support for MSI yet"); }

    bool signal_eoi(uint8) override { return false; }
    bool wait_for_eoi(uint8) override { return false; }

    bool any_irq_active(Vcpu_id);
    bool has_irq_to_inject(Vcpu_id cpu_id) { return highest_irq(cpu_id, false) != nullptr; }
    bool has_irq_in_injection(Vcpu_id cpu_id);
    uint32 highest_irq_to_inject(Vcpu_id cpu_id, uint8 min_priority = PRIORITY_ANY) {
        Irq *irq = highest_irq(cpu_id, false);
        if (irq == nullptr)
            return SPECIAL_INTID_NONE;
        else if (irq->prio() > min_priority)
            return SPECIAL_INTID_NONE;
        else
            return irq->id();
    }

    bool pending_irq(Vcpu_id, Lr &, uint8 min_priority = PRIORITY_ANY);
    void update_inj_status(Vcpu_id cpu_id, uint32 irq_id, IrqState state, bool in_injection);
    void icc_sgi1r_el1(uint64, Vcpu_id);
    bool is_affinity_routing_enabled() const { return _ctlr.affinity_routing(); }
    IRQCtlrVersion version() const { return _version; }

    bool get_irq_info(Vcpu_id id, uint16 irq_id, IrqInfo &info) const {
        if (id >= _num_vcpus)
            return false;
        if (irq_id >= configured_irqs())
            return false;

        const Banked &cpu = _local[id];
        const Irq &irq = irq_object(cpu, irq_id);
        info.active = irq.active();
        info.pending = irq.pending();
        info.enabled = irq.enabled();
        info.in_injection = irq.injection_info.read().in_injection();
        info.priority = irq.prio();
        info.num_asserted = irq.num_asserted;
        info.num_acked = irq.num_acked;

        uint32 aff = irq.routing.aff0() | static_cast<uint32>(irq.routing.aff1() << 8u)
                     | static_cast<uint32>(irq.routing.aff2() << 16u) | static_cast<uint32>(irq.routing.aff3() << 24u);

        if (_ctlr.affinity_routing())
            info.target = irq.routing.any() ? ~0x0u : aff;
        else
            info.target = irq.target();

        return true;
    }

    bool is_irq_in_injection(Vcpu_id id, uint16 irq_id) const {
        if (id >= _num_vcpus)
            return false;
        if (irq_id >= configured_irqs())
            return false;

        const Banked &cpu = _local[id];
        return cpu.in_injection_irqs.is_set(irq_id);
    }

    bool is_irq_in_pending(Vcpu_id id, uint16 irq_id) const {
        if (id >= _num_vcpus)
            return false;
        if (irq_id >= configured_irqs())
            return false;

        const Banked &cpu = _local[id];
        return cpu.pending_irqs.is_set(irq_id);
    }
};

class Model::GicR : public Model::Local_Irq_controller {
private:
    GicD *const _gic_d;
    Vcpu_id const _vcpu_id;
    CpuAffinity const _aff;
    bool const _last;

    struct Waker {
        static constexpr uint32 SLEEP_BIT = 1u << 1;
        static constexpr uint32 CHILDREN_ASLEEP_BIT = 1u << 2;
        static constexpr uint32 RESV_ZERO = ~(SLEEP_BIT | CHILDREN_ASLEEP_BIT);

        uint32 value{SLEEP_BIT | CHILDREN_ASLEEP_BIT};
        constexpr bool sleeping() const { return (value & SLEEP_BIT) != 0u; }
    } _waker;

    bool mmio_write(uint64, uint8, uint64);
    bool mmio_read(uint64, uint8, uint64 &) const;

public:
    GicR(GicD &gic, Vcpu_id cpu_id, CpuAffinity aff, bool last)
        : Local_Irq_controller("GICR"), _gic_d(&gic), _vcpu_id(cpu_id), _aff(aff), _last(last) {}

    Vbus::Err access(Vbus::Access, const VcpuCtx *, Vbus::Space, mword, uint8, uint64 &) override;

    void reset(const VcpuCtx *) override {}

    bool can_receive_irq() const override;

    void assert_vector(uint8 irq_id, bool) override { _gic_d->assert_ppi(_vcpu_id, irq_id); }
    uint8 int_ack() override { ABORT_WITH("interrupt ACK shouldn't be called on the GICR"); }
    bool int_pending(uint8 *) override { return _gic_d->has_irq_in_injection(_vcpu_id) or _gic_d->has_irq_to_inject(_vcpu_id); }

    void nmi_ack() override { ABORT_WITH("NMI ACK shouldn't be called on the GICR"); }
    bool nmi_pending() override {
        return false; // no NMI on ARM
    }
};
