/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#include <model/cpu_affinity.hpp>
#include <platform/compiler.hpp>
#include <platform/new.hpp>
#include <vbus/vbus.hpp>

/*
 * In this file, we are re-using our beloved vbus. While not necessary, it will avoid
 * having to re-build a tree-like struct from scratch here. It does have limitations but
 * those are acceptable for the current implementaton. For example, a lookup based on aff3
 * alone wouldn't be efficient. Mainly, the implementaiton relies on the fact that the user
 * will provide aff3,aff2,aff1 as a cluster ID. If that's not sufficient in the future, we
 * may have to build a complete tree structure.
 */

class CpuClusterWrapper : public Vbus::Device {
public:
    CpuClusterWrapper() : Vbus::Device("CpuCluster") {}

    virtual Vbus::Err access(Vbus::Access, const VcpuCtx*, Vbus::Space, mword, uint8,
                             uint64&) override {
        return Vbus::OK;
    }
    virtual void reset(const VcpuCtx*) override {}

    CpuCluster cluster;
};

static Vbus::Bus affinity_bus(Vbus::AFFINITY);

CpuCluster*
get_cluster_at(CpuAffinity aff) {
    Vbus::Device* vdev
        = affinity_bus.get_device_at(aff.cluster(), CpuCluster::MAX_VCPU_PER_CLUSTER);
    if (__UNLIKELY__(vdev == nullptr))
        return nullptr;

    CpuClusterWrapper* ccw = static_cast<CpuClusterWrapper*>(vdev);

    return &ccw->cluster;
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

    CpuClusterWrapper* cluster_wrap = new (nothrow) CpuClusterWrapper;
    if (cluster_wrap == nullptr)
        return false;

    if (!cluster_wrap->cluster.add_vcpu_id(aff.aff0(), id)) {
        delete cluster_wrap;
        return false;
    }

    if (!affinity_bus.register_device(cluster_wrap, aff.cluster(),
                                      CpuCluster::MAX_VCPU_PER_CLUSTER)) {
        delete cluster_wrap;
        return false;
    }

    return true;
}
