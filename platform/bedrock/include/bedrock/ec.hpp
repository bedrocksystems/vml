/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <zeta/zeta.hpp>

Errno create_ec_resources(uint8*& stack, Nova::Utcb*& utcb);
Errno create_gec(const Zeta::Zeta_ctx* ctx, Cpu cpu, Zeta::global_ec_entry entry_fun,
                 mword arg = 0);
