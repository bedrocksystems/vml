LIBS = vbus irq_controller vcpu_roundup vmm_debug $(PLATFORM) arch_api

$(eval $(call dep_hook,cpu_model,$(LIBS)))
