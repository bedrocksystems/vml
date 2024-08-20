LIBS = irq_controller cpu_model $(PLATFORM)

$(eval $(call dep_hook,timer,$(LIBS)))
