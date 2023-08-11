LIBS = vbus $(PLATFORM) cpu_model

$(eval $(call dep_hook,irq_controller,$(LIBS)))
