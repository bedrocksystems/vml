/**
 * Copyright (C) 2021-2025 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/types.hpp>

class CpuAffinity {
public:
    explicit CpuAffinity(uint32 aff) : _aff(aff) {}

    // Construct from 64-bit MPIDR register.
    explicit CpuAffinity(uint64 mpidr_el1)
        : _aff(static_cast<uint32>(((mpidr_el1 >> 8) & 0xff000000ull)) | static_cast<uint32>((mpidr_el1 & 0xffffffull))) {}

    static CpuAffinity from_vcpu_id(uint64 vcpu_id) {
        // Construct affinity from logical cpu ID
        // This supports 256 clusters of 16 vcpus each.
        return CpuAffinity{static_cast<uint32>(((vcpu_id / 16) << 8) | (vcpu_id % 16))};
    }

    // Retrieve 64-bit MPIDR from packed affinity value.
    uint64 mpidr() const {
        return ((static_cast<uint64>(_aff) << 8) & 0xff00000000ull) | (static_cast<uint64>(_aff) & 0xffffffull);
    }

    uint8 aff0() const { return static_cast<uint8>(_aff); }
    uint8 aff1() const { return static_cast<uint8>(_aff >> 8); }
    uint8 aff2() const { return static_cast<uint8>(_aff >> 16); }
    uint8 aff3() const { return static_cast<uint8>(_aff >> 24); }

    uint32 cluster() const { return _aff & ~(0xFFu); }

    uint32 affinity() const { return _aff; }

private:
    uint32 _aff;
};

class CpuCluster {
public:
    // NOLINTNEXTLINE (cppcoreguidelines-pro-type-member-init) - tidy is wrong there
    CpuCluster() {
        for (auto& e : _vcpus)
            e = INVALID_VCPU_ID;
    }

    static constexpr uint8 MAX_VCPU_PER_CLUSTER = 16;

    Vcpu_id vcpu_id(uint8 id_in_cluster) const {
        if (id_in_cluster >= MAX_VCPU_PER_CLUSTER)
            return INVALID_VCPU_ID;

        return _vcpus[id_in_cluster];
    }

    bool add_vcpu_id(uint8 id_in_cluster, Vcpu_id vid) {
        if (id_in_cluster >= MAX_VCPU_PER_CLUSTER)
            return false;

        _vcpus[id_in_cluster] = vid;
        return true;
    }

private:
    Vcpu_id _vcpus[MAX_VCPU_PER_CLUSTER];
};

Vcpu_id cpu_affinity_to_id(CpuAffinity aff);
CpuCluster* cpu_affinity_to_cluster(CpuAffinity aff);
bool add_cpu_with_affinity(Vcpu_id id, CpuAffinity aff);
