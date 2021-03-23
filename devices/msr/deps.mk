LIBS = vbus cpu_model timer gic irq_controller vmm_debug vmi_interface simple_as dynamic_as $(PLATFORM) arch_api

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log zeta nova pt alloc msc uuid concurrent
endif

$(eval $(call dep_hook,msr,$(LIBS)))
