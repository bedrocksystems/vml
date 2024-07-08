/*
 * Copyright (C) 2023 BlueRock Security, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BlueRock Open-Source License.
 * See the LICENSE-BlueRock file in the repository root for details.
 */

#include <debug_switches.hpp>

enum Debug::Level Debug::current_level = Debug::NONE;
bool Stats::requested = false;
