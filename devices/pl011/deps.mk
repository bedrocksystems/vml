LIBS = vbus irq_controller cpu_model $(PLATFORM) arch_api

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log nova
endif

$(eval $(call dep_hook,pl011,$(LIBS)))
