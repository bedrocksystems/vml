LIBS = vbus cpu_model timer gic vmm_debug vmi_interface simple_as dynamic_as $(PLATFORM) arch_api

ifeq ($(PLATFORM), bedrock)

LIBS += lang cxx log zeta nova pt alloc msc uuid

# vmi libs
ifneq ($(ENABLE_VMI), 0)
LIBS += beam matter outpost
else
LIBS += outpost_dummy
endif

else
LIBS += outpost_dummy
endif

$(eval $(call dep_hook,msr,$(LIBS)))
