/*
 * Copyright (c) 2023 BedRock Systems, Inc.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

#include <memory>

template<typename T>
using unique_ptr = std::unique_ptr<T>;
