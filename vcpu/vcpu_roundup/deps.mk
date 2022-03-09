LIBS = cpu_model $(PLATFORM) arch_api vmm_debug

$(eval $(call dep_hook,vcpu_roundup,$(LIBS)))
