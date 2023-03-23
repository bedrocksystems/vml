/**
 * Copyright (C) 2021 BedRock Systems, Inc.
 * All rights reserved.
 *
 * This software is distributed under the terms of the BedRock Open-Source License.
 * See the LICENSE-BedRock file in the repository root for details.
 */

#pragma once

#include <model/virtio_common.hpp>
#include <platform/types.hpp>

namespace Virtio {
    class MMIOTransport;
};

class Virtio::MMIOTransport final : public Virtio::Transport {
private:
    enum {
        RO_MAGIC = 0x0,
        RO_MAGIC_END = 0x3,
        RO_VERSION = 0x4,
        RO_VERSION_END = 0x7,
        RO_DEVICE_ID = 0x8,
        RO_DEVICE_ID_END = 0xb,
        RO_VENDOR_ID = 0xc,
        RO_VENDOR_ID_END = 0xf,
        RO_DEVICE_FEATURE = 0x10,
        RO_DEVICE_FEATURE_END = 0x13,
        RW_DEVICE_FEATURE_SEL = 0x14,
        RW_DEVICE_FEATURE_SEL_END = 0x17,

        WO_DRIVER_FEATURE = 0x20,
        WO_DRIVER_FEATURE_END = 0x23,
        RW_DRIVER_FEATURE_SEL = 0x24,
        RW_DRIVER_FEATURE_SEL_END = 0x27,

        WO_QUEUE_SEL = 0x30,
        WO_QUEUE_SEL_END = 0x33,
        RO_QUEUE_NUM_MAX = 0x34,
        RO_QUEUE_NUM_MAX_END = 0x37,
        WO_QUEUE_NUM = 0x38,
        WO_QUEUE_NUM_END = 0x3b,

        RW_QUEUE_READY = 0x44,
        RW_QUEUE_READY_END = 0x47,

        WO_QUEUE_NOTIFY = 0x50,
        RO_IRQ_STATUS = 0x60,
        RO_IRQ_STATUS_END = 0x63,
        WO_IRQ_ACK = 0x64,
        WO_IRQ_ACK_END = 0x67,

        RW_STATUS = 0x70,
        RW_STATUS_END = 0x73,

        WO_QUEUE_DESCR_LOW = 0x80,
        WO_QUEUE_DESCR_LOW_END = 0x83,
        WO_QUEUE_DESCR_HIGH = 0x84,
        WO_QUEUE_DESCR_HIGH_END = 0x87,

        WO_QUEUE_DRIVER_LOW = 0x90,
        WO_QUEUE_DRIVER_LOW_END = 0x93,
        WO_QUEUE_DRIVER_HIGH = 0x94,
        WO_QUEUE_DRIVER_HIGH_END = 0x97,

        WO_QUEUE_DEVICE_LOW = 0xa0,
        WO_QUEUE_DEVICE_LOW_END = 0xa3,
        WO_QUEUE_DEVICE_HIGH = 0xa4,
        WO_QUEUE_DEVICE_HIGH_END = 0xa7,

        RO_CONFIG_GENERATION = 0xfc,
        RO_CONFIG_GENERATION_END = 0xff,

        RW_CONFIG = 0x100,
        RW_CONFIG_END = 0x163,
    };

public:
    MMIOTransport() {}

    static bool read(uint64 const offset, uint8 const bytes, uint64 &value, const Virtio::DeviceState &state) {
        if (bytes > 4)
            return false;

        switch (offset) {
        case RO_MAGIC ... RO_MAGIC_END:
            return Virtio::read_register(offset, RO_MAGIC, RO_MAGIC_END, bytes, 0x74726976ULL, value);
        case RO_VERSION ... RO_VERSION_END:
            return Virtio::read_register(offset, RO_VERSION, RO_VERSION_END, bytes, 2, value);
        case RO_DEVICE_ID ... RO_DEVICE_ID_END:
            return Virtio::read_register(offset, RO_DEVICE_ID, RO_DEVICE_ID_END, bytes, state.device_id, value);
        case RO_VENDOR_ID ... RO_VENDOR_ID_END:
            return Virtio::read_register(offset, RO_VENDOR_ID, RO_VENDOR_ID_END, bytes, state.vendor_id, value);
        case RO_DEVICE_FEATURE ... RO_DEVICE_FEATURE_END:
            if (state.drv_device_sel == 0)
                return Virtio::read_register(offset, RO_DEVICE_FEATURE, RO_DEVICE_FEATURE_END, bytes, state.device_feature_lower,
                                             value);
            else
                return Virtio::read_register(offset, RO_DEVICE_FEATURE, RO_DEVICE_FEATURE_END, bytes, 1 /*VIRTIO_F_VERSION_1*/,
                                             value);
        case RW_DEVICE_FEATURE_SEL ... RW_DEVICE_FEATURE_SEL_END:
            return Virtio::read_register(offset, RW_DEVICE_FEATURE_SEL, RW_DEVICE_FEATURE_SEL_END, bytes, state.drv_device_sel,
                                         value);
        case RW_DRIVER_FEATURE_SEL ... RW_DRIVER_FEATURE_SEL_END:
            return Virtio::read_register(offset, RW_DRIVER_FEATURE_SEL, RW_DRIVER_FEATURE_SEL_END, bytes, state.drv_feature_sel,
                                         value);
        case RO_QUEUE_NUM_MAX ... RO_QUEUE_NUM_MAX_END:
            return Virtio::read_register(offset, RO_QUEUE_NUM_MAX, RO_QUEUE_NUM_MAX_END, bytes, state.queue_num_max, value);
        case RW_QUEUE_READY ... RW_QUEUE_READY_END:
            return Virtio::read_register(offset, RW_QUEUE_READY, RW_QUEUE_READY_END, bytes, state.selected_queue_data().ready,
                                         value);
        case RO_IRQ_STATUS ... RO_IRQ_STATUS_END:
            return Virtio::read_register(offset, RO_IRQ_STATUS, RO_IRQ_STATUS_END, bytes, state.irq_status, value);
        case RW_STATUS ... RW_STATUS_END:
            return Virtio::read_register(offset, RW_STATUS, RW_STATUS_END, bytes, state.status, value);

        case RO_CONFIG_GENERATION ... RO_CONFIG_GENERATION_END:
            return Virtio::read_register(offset, RO_CONFIG_GENERATION, RO_CONFIG_GENERATION_END, bytes, state.get_config_gen(),
                                         value);
        case RW_CONFIG ... RW_CONFIG_END: {
            if (offset + bytes > RW_CONFIG_END)
                return false;

            return config_space_read(offset, RW_CONFIG, bytes, value, state);
        }
        }
        return false;
    }

    static bool write(uint64 const offset, uint8 const bytes, uint64 const value, Virtio::DeviceState &state) {
        if (bytes > 4)
            return false;

        switch (offset) {
        case RW_DEVICE_FEATURE_SEL ... RW_DEVICE_FEATURE_SEL_END:
            return Virtio::write_register(offset, RW_DEVICE_FEATURE_SEL, RW_DEVICE_FEATURE_SEL_END, bytes, value,
                                          state.drv_device_sel);
        case WO_DRIVER_FEATURE ... WO_DRIVER_FEATURE_END:
            if (state.drv_feature_sel == 0)
                return Virtio::write_register(offset, WO_DRIVER_FEATURE, WO_DRIVER_FEATURE_END, bytes, value,
                                              state.drv_feature_lower);
            else
                return Virtio::write_register(offset, WO_DRIVER_FEATURE, WO_DRIVER_FEATURE_END, bytes, value,
                                              state.drv_feature_upper);
        case RW_DRIVER_FEATURE_SEL ... RW_DRIVER_FEATURE_SEL_END:
            return Virtio::write_register(offset, RW_DRIVER_FEATURE_SEL, RW_DRIVER_FEATURE_SEL_END, bytes, value,
                                          state.drv_feature_sel);
        case WO_QUEUE_SEL ... WO_QUEUE_SEL_END:
            if (value >= static_cast<uint8>(Virtio::Queues::MAX))
                return true; /* ignore out of bound */
            return Virtio::write_register(offset, WO_QUEUE_SEL, WO_QUEUE_SEL_END, bytes, value, state.sel_queue);
        case WO_QUEUE_NUM ... WO_QUEUE_NUM_END:
            if (value > state.queue_num_max)
                return true; /* ignore out of bound */
            return Virtio::write_register(offset, WO_QUEUE_NUM, WO_QUEUE_NUM_END, bytes, value, state.selected_queue_data().num);
        case RW_QUEUE_READY ... RW_QUEUE_READY_END: {
            if (!Virtio::write_register(offset, RW_QUEUE_READY, RW_QUEUE_READY_END, bytes, value,
                                        state.selected_queue_data().ready))
                return false;
            state.construct_queue = true;
            return true;
        }
        case WO_IRQ_ACK ... WO_IRQ_ACK_END:
            state.irq_acknowledged = true;
            return true;
        case RW_STATUS ... RW_STATUS_END:
            state.status_changed = true;
            return Virtio::write_register(offset, RW_STATUS, RW_STATUS_END, bytes, value, state.status);
        case WO_QUEUE_NOTIFY:
            state.notify = true;
            state.notify_val = static_cast<uint32>(value);
            return true;
        case WO_QUEUE_DESCR_LOW ... WO_QUEUE_DESCR_LOW_END:
            return Virtio::write_register(offset, WO_QUEUE_DESCR_LOW, WO_QUEUE_DESCR_LOW_END, bytes, value,
                                          state.selected_queue_data().descr_low);
        case WO_QUEUE_DESCR_HIGH ... WO_QUEUE_DESCR_HIGH_END:
            return Virtio::write_register(offset, WO_QUEUE_DESCR_HIGH, WO_QUEUE_DESCR_HIGH_END, bytes, value,
                                          state.selected_queue_data().descr_high);
        case WO_QUEUE_DRIVER_LOW ... WO_QUEUE_DRIVER_LOW_END:
            return Virtio::write_register(offset, WO_QUEUE_DRIVER_LOW, WO_QUEUE_DRIVER_LOW_END, bytes, value,
                                          state.selected_queue_data().driver_low);
        case WO_QUEUE_DRIVER_HIGH ... WO_QUEUE_DRIVER_HIGH_END:
            return Virtio::write_register(offset, WO_QUEUE_DRIVER_HIGH, WO_QUEUE_DRIVER_HIGH_END, bytes, value,
                                          state.selected_queue_data().driver_high);
        case WO_QUEUE_DEVICE_LOW ... WO_QUEUE_DEVICE_LOW_END:
            return Virtio::write_register(offset, WO_QUEUE_DEVICE_LOW, WO_QUEUE_DEVICE_LOW_END, bytes, value,
                                          state.selected_queue_data().device_low);
        case WO_QUEUE_DEVICE_HIGH ... WO_QUEUE_DEVICE_HIGH_END:
            return Virtio::write_register(offset, WO_QUEUE_DEVICE_HIGH, WO_QUEUE_DEVICE_HIGH_END, bytes, value,
                                          state.selected_queue_data().device_high);
        // Config space access can be byte aligned.
        case RW_CONFIG ... RW_CONFIG_END: {
            if (offset + bytes > RW_CONFIG_END)
                return false;

            return config_space_write(offset, RW_CONFIG, bytes, value, state);
        }
        }
        return false;
    }

    virtual bool access(Vbus::Access const access, mword const offset, uint8 const size, uint64 &value,
                        Virtio::DeviceState &state) override {
        if (access == Vbus::Access::WRITE)
            return Virtio::MMIOTransport::write(offset, size, value, state);
        if (access == Vbus::Access::READ)
            return Virtio::MMIOTransport::read(offset, size, value, state);

        return false;
    }

    virtual void assert_queue_interrupt(Model::Irq_controller *const irq_ctrlr, uint16 irq, Virtio::DeviceState &state) override {
        /* At this point, the guest has not yet acknowledged the exisiting interrupt OR
           deassert_queue_interrupt is being executed from other context. In both cases, the guest
           still needs to process the queues once it returns from vmexit and it is safe to skip
           injecting a new interrupt. */
        if ((state.irq_status & 0x1) != 0u)
            return;

        // The interrupt has been deasserted/acknowledged at this point (happens once) so we need to
        // inject a new one.
        state.irq_status.or_fetch(0x1);
        irq_ctrlr->assert_global_line(irq);
    }

    virtual void deassert_queue_interrupt(Model::Irq_controller *const irq_ctrlr, uint16 irq,
                                          Virtio::DeviceState &state) override {
        irq_ctrlr->deassert_global_line(irq);
        state.irq_status.and_fetch(static_cast<uint32>(~0x1));
    }

    virtual void assert_config_change_interrupt(Model::Irq_controller *const irq_ctrlr, uint16 irq,
                                                Virtio::DeviceState &state) override {
        if ((state.irq_status & 0x2) != 0u)
            return;

        state.irq_status.or_fetch(0x2);
        irq_ctrlr->assert_global_line(irq);
    }

    virtual void deassert_config_change_interrupt(Model::Irq_controller *const irq_ctrlr, uint16 irq,
                                                  Virtio::DeviceState &state) override {
        irq_ctrlr->deassert_global_line(irq);
        state.irq_status.and_fetch(static_cast<uint32>(~0x2));
    }
};