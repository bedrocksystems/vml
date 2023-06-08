/*
 * Copyright (C) 2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */
#pragma once

/*! \file Exposes error code compatible with Zeta
 *
 *  We borrow the error codes from the standard errno.h and
 *  redefine Errno::NONE since this is the only one that is not standard.
 */

#include <errno.h>
#include <platform/types.hpp>

#ifndef EBADR     // Some platforms (BSD-derived) don't have this one
#define EBADR 200 // Pick a high number that won't collide with something else
#endif

enum class Errno {
    NONE = 0,
    PERM = EPERM,
    NOENT = ENOENT,
    AGAIN = EAGAIN,
    NOMEM = ENOMEM,
    RBUSY = EBUSY,
    EXIST = EEXIST,
    INVAL = EINVAL,
    BADR = EBADR,
    NOTRECOVERABLE = ENOTRECOVERABLE,
};
