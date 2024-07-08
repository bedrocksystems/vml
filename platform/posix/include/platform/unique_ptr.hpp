/*
 * Copyright (c) 2023 BlueRock Security, Inc.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */
#pragma once

#include <memory>

template<typename T>
using unique_ptr = std::unique_ptr<T>;
