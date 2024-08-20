LIBS = vbus virtio_base cpu_model irq_controller $(PLATFORM)

$(eval $(call dep_hook,virtio_block,$(LIBS)))
