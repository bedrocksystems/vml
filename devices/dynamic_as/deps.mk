LIBS = vbus arch_api cpu_model $(PLATFORM) vmm_debug vmi_interface simple_as

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log fdt zeta nova pt msc uuid alloc
endif

# vmi libs
ifneq ($(ENABLE_VMI), 0)
LIBS += beam matter outpost
else
LIBS += outpost_dummy
endif

$(eval $(call dep_hook,dynamic_as,$(LIBS)))
