LIBS = vbus cpu_model vmm_debug irq_controller simple_as $(PLATFORM)

$(eval $(call dep_hook,gic,$(LIBS)))
