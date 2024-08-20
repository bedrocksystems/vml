LIBS = vbus virtio_base irq_controller cpu_model $(PLATFORM)

$(eval $(call dep_hook,virtio_net,$(LIBS)))
