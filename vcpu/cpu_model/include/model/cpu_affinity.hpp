/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <model/vcpu_types.hpp>
#include <platform/types.hpp>

class CpuAffinity {
public:
    explicit CpuAffinity(uint32 aff) : _aff(aff) {}

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
    CpuCluster() {
        for (auto& e : _vcpus)
            e = INVALID_VCPU_ID;
    }

    // Limit to 16 CPUs at the moment. Could change in the future.
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
