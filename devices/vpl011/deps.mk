LIBS = vbus irq_controller $(PLATFORM) vuart vmm_debug arch_api

$(eval $(call dep_hook,vpl011,$(LIBS)))
