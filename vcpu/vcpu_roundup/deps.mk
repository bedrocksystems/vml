LIBS = cpu_model arch_api vmm_debug
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif

$(eval $(call dep_hook,vcpu_roundup,$(LIBS)))
