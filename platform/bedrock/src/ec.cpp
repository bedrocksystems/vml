/**
 * Copyright (C) 2019 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <alloc/vmap.hpp>
#include <bedrock/ec.hpp>
#include <log/log.hpp>
#include <new.hpp>

#define ZETA_DEFAULT_STACK_SIZE 4096

Errno
create_ec_resources(uint8 *&stack, Nova::Utcb *&utcb) {
    stack = new (nothrow) uint8[ZETA_DEFAULT_STACK_SIZE];
    if (stack == nullptr)
        return Errno::ENOMEM;

    utcb = reinterpret_cast<Nova::Utcb *>(Vmap::pagealloc());
    if (utcb == nullptr) {
        delete stack;
        return Errno::ENOMEM;
    }

    stack += ZETA_DEFAULT_STACK_SIZE;

    return Errno::ENONE;
}

Errno
create_gec(const Zeta::Zeta_ctx *ctx, Cpu cpu, Zeta::global_ec_entry entry_fun, mword arg) {
    Errno err;
    uint8 *stack;
    Nova::Utcb *utcb;

    err = create_ec_resources(stack, utcb);
    if (err != ENONE)
        return err;

    Sel ec_sel = Sels::alloc();
    err = Zeta::create_global_ec(ctx, Nova::USE_NONE, ec_sel, cpu, reinterpret_cast<mword>(stack),
                                 utcb, reinterpret_cast<Zeta::global_ec_entry>(entry_fun),
                                 reinterpret_cast<void *>(arg));
    if (err != Errno::ENONE) {
        WARN("create_global_ec failed with %u", err);
        return err;
    }

    err = Zeta::create_sc(ctx, Sels::alloc(), ec_sel, Nova::Qpd());
    if (err != Errno::ENONE) {
        WARN("create_sc failed with %u", err);
        return err;
    }

    return Errno::ENONE;
}
