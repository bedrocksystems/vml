/**
 * Copyright (C) 2019-2020 BedRock Systems, Inc.
 * All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <platform/errno.hpp>
#include <platform/types.hpp>

namespace Zeta {
    class Zeta_ctx;
}

namespace Model {
    class Board_impl;
    class Guest_as;
    class Cpu;
    class Board;
    class Gic_d;
    class Firmware;
}

namespace Fdt {
    class Tree;
}

namespace Vbus {
    class Bus;
}

struct Uuid;

class Model::Board {
public:
    Board() {}
    Errno init(const Zeta::Zeta_ctx *ctx, const Fdt::Tree &tree, Model::Guest_as &ram_as,
               Model::Guest_as &rom_as, const mword guest_config_addr, const Uuid &umx_uuid,
               const Uuid &vswitch_uuid, const Uuid &plat_mgr_uuid);
    bool setup_gicr(Model::Cpu &, const Fdt::Tree &);

    Model::Gic_d *get_gic() const;
    Model::Firmware *get_firmware() const;
    Vbus::Bus *get_bus() const;

private:
    Model::Board_impl *impl{nullptr};
};
