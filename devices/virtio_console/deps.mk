LIBS = vbus virtio_base
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif

$(eval $(call dep_hook,virtio_console,$(LIBS)))
