# vmm libs - devices
LIBS += vbus gic irq_controller timer virtio_console virtio_net virtio_base simple_as

# vmm libs - config
LIBS += vmm_debug

# vmm libs - vcpu
LIBS += cpu_model

# vmm libs - platform
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
else
LIBS += platform_wrapper
endif

# vmm libs - arch
LIBS += arch_api

$(eval $(call dep_hook,virtio_posix,$(LIBS)))
