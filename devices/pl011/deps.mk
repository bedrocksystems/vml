LIBS = vbus irq_controller cpu_model $(PLATFORM) arch_api vuart vmm_debug

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log nova zeta msc pt alloc uuid concurrent
endif

$(eval $(call dep_hook,pl011,$(LIBS)))
