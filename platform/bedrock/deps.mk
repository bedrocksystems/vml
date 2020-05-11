LIBS = lang cxx zeta nova pt msc uuid alloc log vmm_debug vm_config cpu_model vcpu_bedrock vmi_interface board vcpu_roundup

# deps of the vcpu lib
LIBS += timer msr

# comes for the vswitch and umx interfaces - we could move that in another lib later on
LIBS += virtio virtio_console virtio_net gic vbus pl011 io umx concurrent

# comes from using the board - will be improved once the RAM is regular vbus devive
LIBS += simple_as dynamic_as

# due to board, will go away
LIBS += fdt firmware

# vmi libs
ifneq ($(ENABLE_VMI), 0)
LIBS += beam matter outpost
else
LIBS += outpost_dummy
endif

$(eval $(call dep_hook,bedrock,$(LIBS)))
