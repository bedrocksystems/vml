LIBS = lang cxx vbus log cpu_model pm_client board gic vmm_debug bedrock alloc nova zeta pt msc uuid

# To parse the device config
LIBS += fdt

$(eval $(call dep_hook,passthrough,$(LIBS)))
