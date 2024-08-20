LIBS = vbus virtio_base irq_controller cpu_model
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif

$(eval $(call dep_hook,virtio_net,$(LIBS)))
