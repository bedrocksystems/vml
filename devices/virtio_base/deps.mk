LIBS = vbus irq_controller simple_as $(PLATFORM)

$(eval $(call dep_hook,virtio_base,$(LIBS)))
