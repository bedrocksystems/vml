LIBS = vbus virtio_base cpu_model irq_controller
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif

$(eval $(call dep_hook,virtio_sock,$(LIBS)))
