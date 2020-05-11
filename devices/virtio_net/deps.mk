LIBS = lang cxx vbus gic cpu_model virtio vmm_debug $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += uuid msc io pt zeta nova concurrent alloc umx lang cxx log
endif


$(eval $(call dep_hook,virtio_net,$(LIBS)))
