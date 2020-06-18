LIBS = vbus cpu_model vmm_debug irq_controller $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log nova zeta pt alloc msc uuid
endif

$(eval $(call dep_hook,gic,$(LIBS)))
