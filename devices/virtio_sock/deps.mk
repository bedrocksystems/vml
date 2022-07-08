LIBS = vbus virtio_base $(PLATFORM)

$(eval $(call dep_hook,virtio_sock,$(LIBS)))
