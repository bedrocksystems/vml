/**
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <model/simple_as.hpp>
#include <platform/bitset.hpp>
#include <platform/types.hpp>

namespace Vmm::Vcpu {
    typedef uint64 Vcpu_id;
    typedef mword Cpu_id;

    void ctrl_tvm(Vcpu_id id, bool enable, uint64 regs);

    uint16 get_num_vcpus();
    Cpu_id get_pcpu(Vcpu_id);

    template<size_t SIZE>
    bool pcpus_in_use(Bitset<SIZE>& bitset) {
        bitset.reset();

        for (uint32 i = 0; i < get_num_vcpus(); i++) {
            Cpu_id cur_cpu = get_pcpu(i);

            if (cur_cpu >= bitset.size())
                return false;

            // Update bitmap
            bitset.atomic_set(cur_cpu);
        }

        return true;
    }

    namespace Roundup {
        void roundup();
        void roundup_from_vcpu(Vcpu_id vcpu_id);
        void resume();
    };
}

namespace Vmm::Msr {
    enum Trap_id {
        UNKNOWN,
        TTBR0_EL1,
        TTBR1_EL1,
        TCR_EL1,
    };

    struct Trap_info {
        bool read;
        Msr::Trap_id id;
        const char* name;
        uint64 cur_value;
        uint64 new_value;
    };
}

namespace Vmm::Pf {
    /* XXX: eventually, this will go away. But, since we don't decode
     * and emulate instructions, we have to work with partial info.
     */
    static constexpr uint64 SIZE_INFO_INVALID = ~0x0ull;

    struct Access_info {
        Page_permission type;
        uint64 gpa;
        uint64 size;
    };
};