LIBS = vbus irq_controller vuart vmm_debug arch_api
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif


$(eval $(call dep_hook,vpl011,$(LIBS)))
