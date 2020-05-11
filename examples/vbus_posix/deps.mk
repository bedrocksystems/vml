# vmm libs - devices
LIBS += vbus pl011 gic timer simple_as msr firmware

# vmm libs - config
LIBS += vmm_debug

# vmm libs - vcpu
LIBS += cpu_model vcpu vcpu_roundup

# vmm libs - platform
LIBS += $(PLATFORM)

# vmm libs - arch
LIBS += arch_api

$(eval $(call dep_hook,vbus_posix,$(LIBS)))
