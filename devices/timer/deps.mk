LIBS = irq_controller cpu_model
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
else
endif

$(eval $(call dep_hook,timer,$(LIBS)))
