LIBS = vbus gic cpu_model virtio vmm_debug $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += zeta nova pt msc uuid alloc lang cxx io log
endif

$(eval $(call dep_hook,virtio_console,$(LIBS)))
