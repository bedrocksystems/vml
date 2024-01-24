/**
 * Copyright (C) 2024 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <platform/types.hpp>

static constexpr uint32 PROC_CNTR0_CR3_LOAD = 1u << 15;
static constexpr uint32 PROC_CNTR0_TPR_SHADOW = 1u << 21;
static constexpr uint32 PROC_CNTR0_MTF = 1u << 27;

static constexpr uint32 PROC_CNTR1_VIRT_APIC_ACCESS = 1u << 0;
static constexpr uint32 PROC_CNTR1_IDT_GDT = 1u << 2;
static constexpr uint32 PROC_CNTR1_ENABLE_RDTSCP = 1u << 3;
static constexpr uint32 PROC_CNTR1_VIRT_X2APIC_MODE = 1u << 4;
static constexpr uint32 PROC_CNTR1_APIC_REG_VIRT = 1u << 8;
static constexpr uint32 PROC_CNTR1_VIRT_IRQ_DELIVERY = 1u << 9;
static constexpr uint32 PROC_CNTR1_ENABLE_INVPCID = 1u << 12;
static constexpr uint32 PROC_CNTR1_VIRT_XSAVES = 1u << 20;
static constexpr uint32 PROC_CNTR1_MBEC = 1u << 22;
