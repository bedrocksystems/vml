LIBS = vbus irq_controller vcpu_roundup vmm_debug arch_api
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif


$(eval $(call dep_hook,cpu_model,$(LIBS)))
