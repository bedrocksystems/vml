LIBS = vbus irq_controller simple_as cpu_model arch_api
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif

$(eval $(call dep_hook,virtio_base,$(LIBS)))
