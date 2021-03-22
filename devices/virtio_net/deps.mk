LIBS = lang cxx vbus irq_controller cpu_model virtio_base simple_as $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += uuid msc io pt zeta nova concurrent alloc umx lang cxx log
endif


$(eval $(call dep_hook,virtio_net,$(LIBS)))
