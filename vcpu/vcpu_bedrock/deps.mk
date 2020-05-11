LIBS = cxx lang pt nova alloc fdt io uuid zeta umx msc concurrent log pm_client vbus pl011 cpu_model vmi_interface vcpu_roundup
LIBS += gic vmm_debug msr timer bedrock arch_api simple_as dynamic_as board guest_config

# vmi libs
ifneq ($(ENABLE_VMI), 0)
LIBS += beam matter outpost
else
LIBS += outpost_dummy
endif

$(eval $(call dep_hook,vcpu_bedrock,$(LIBS)))
