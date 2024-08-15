# vmm libs - devices
LIBS += vbus vpl011 gic irq_controller timer simple_as msr vuart

# vmm libs - config
LIBS += vmm_debug

# vmm libs - vcpu
LIBS += cpu_model vcpu vcpu_roundup

# vmm libs - platform
LIBS += $(PLATFORM)

# vmm libs - arch
LIBS += arch_api

$(eval $(call dep_hook,vbus_posix,$(LIBS)))
