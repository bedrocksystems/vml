LIBS = vbus irq_controller $(PLATFORM) vuart vmm_debug

$(eval $(call dep_hook,vpl011,$(LIBS)))
