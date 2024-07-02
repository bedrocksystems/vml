LIBS = vbus cpu_model vmm_debug irq_controller
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif

$(eval $(call dep_hook,gic,$(LIBS)))
