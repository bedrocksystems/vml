/**
 * Copyright (C) 2021-2024 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <model/cpu_affinity.hpp>
#include <model/vcpu_types.hpp>
#include <platform/compiler.hpp>
#include <platform/log.hpp>
#include <platform/new.hpp>
#include <platform/rangemap.hpp>

// should be fine uint32 but current Range implementation does not like this
typedef uint64 aff_type;
struct CpuClusterPtr final : public RangeNode<aff_type> {
    explicit CpuClusterPtr(const Range<aff_type>& r) : RangeNode(r) {}
    CpuCluster cluster;
};

static RangeMap<aff_type> clusters_map;

CpuCluster*
get_cluster_at(CpuAffinity aff) {
    Range<aff_type> r(aff.cluster(), CpuCluster::MAX_VCPU_PER_CLUSTER);
    CpuClusterPtr* ptr = static_cast<CpuClusterPtr*>(clusters_map.lookup(&r));
    return ptr == nullptr ? nullptr : &ptr->cluster;
}

Vcpu_id
cpu_affinity_to_id(CpuAffinity aff) {
    CpuCluster* cluster = get_cluster_at(aff);

    if (__UNLIKELY__(cluster == nullptr))
        return INVALID_VCPU_ID;

    return cluster->vcpu_id(aff.aff0());
}

CpuCluster*
cpu_affinity_to_cluster(CpuAffinity aff) {
    return get_cluster_at(aff);
}

bool
add_cpu_with_affinity(Vcpu_id id, CpuAffinity aff) {
    CpuCluster* cluster = get_cluster_at(aff);

    if (cluster != nullptr)
        return cluster->add_vcpu_id(aff.aff0(), id);

    Range<aff_type> r(aff.cluster(), CpuCluster::MAX_VCPU_PER_CLUSTER);

    CpuClusterPtr* ptr = new (nothrow) CpuClusterPtr(r);
    if (ptr == nullptr) {
        WARN("not enough memory for CpuClusterPtr");
        return false;
    }

    if (!ptr->cluster.add_vcpu_id(aff.aff0(), id)) {
        delete ptr;
        return false;
    }

    if (!clusters_map.insert(ptr)) {
        delete ptr;
        return false;
    }

    return true;
}
