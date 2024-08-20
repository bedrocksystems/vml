LIBS = vbus cpu_model

ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif

ifneq ($(BOARD), x86)
LIBS += gic irq_controller simple_as timer
endif

ifeq ($(PLATFORM), bluerock)
LIBS += intrusive
endif

$(eval $(call dep_hook,msr,$(LIBS)))
