LIBS = vbus irq_controller vuart cpu_model $(PLATFORM)

$(eval $(call dep_hook,vpl011,$(LIBS)))
