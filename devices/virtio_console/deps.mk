LIBS = vbus irq_controller cpu_model virtio_base simple_as $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += zeta nova pt msc uuid alloc lang cxx io log concurrent
endif

$(eval $(call dep_hook,virtio_console,$(LIBS)))
