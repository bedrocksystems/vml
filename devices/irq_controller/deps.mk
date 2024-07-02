LIBS = vbus cpu_model
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif


$(eval $(call dep_hook,irq_controller,$(LIBS)))
