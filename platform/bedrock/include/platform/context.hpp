/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/*! \file Define a platform dependent context information
 *
 * This file should expose an opaque Platform_ctx type
 */

namespace Zeta {
    class Zeta_ctx;
}

using Platform_ctx = Zeta::Zeta_ctx;