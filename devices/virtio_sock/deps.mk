LIBS = vbus virtio_base irq_controller $(PLATFORM)

$(eval $(call dep_hook,virtio_sock,$(LIBS)))
