LIBS = vbus virtio_base irq_controller simple_as cpu_model $(PLATFORM)

$(eval $(call dep_hook,virtio_console,$(LIBS)))
