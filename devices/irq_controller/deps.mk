LIBS = vbus cpu_model $(PLATFORM)

$(eval $(call dep_hook,irq_controller,$(LIBS)))
