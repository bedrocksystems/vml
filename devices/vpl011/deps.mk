LIBS = vbus irq_controller vuart $(PLATFORM)

$(eval $(call dep_hook,vpl011,$(LIBS)))
