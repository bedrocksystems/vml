LIBS = vbus cpu_model vmm_debug irq_controller $(PLATFORM)

$(eval $(call dep_hook,gic,$(LIBS)))
