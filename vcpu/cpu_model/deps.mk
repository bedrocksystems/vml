LIBS = vbus irq_controller vcpu_roundup vmm_debug $(PLATFORM) arch_api

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log nova zeta pt alloc msc uuid concurrent
endif

$(eval $(call dep_hook,cpu_model,$(LIBS)))
