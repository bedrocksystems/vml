LIBS = vbus irq_controller simple_as arch_api
LIBS += $(PLATFORM)

$(eval $(call dep_hook,virtio_base,$(LIBS)))
