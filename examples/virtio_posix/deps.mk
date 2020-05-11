# vmm libs - devices
LIBS += vbus gic timer virtio_console virtio_net virtio

# vmm libs - config
LIBS += vmm_debug

# vmm libs - vcpu
LIBS += cpu_model

# vmm libs - platform
LIBS += $(PLATFORM)

# vmm libs - arch
LIBS += arch_api

$(eval $(call dep_hook,virtio_posix,$(LIBS)))
