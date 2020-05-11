LIBS  = lang cxx vbus cpu_model log timer gic simple_as dynamic_as fdt vmm_debug
LIBS += firmware passthrough pl011 pm_client virtio_console virtio_net virtio vm_config

# Deps of PM
LIBS += zeta nova pt msc uuid alloc

# Deps of UMX interface
LIBS += io umx concurrent

LIBS += bedrock

$(eval $(call dep_hook,board,$(LIBS)))
