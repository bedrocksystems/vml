LIBS = vbus cpu_model
LIBS += $(PLATFORM)

ifeq ($(ARCH), aarch64)
LIBS += gic irq_controller simple_as timer
endif

ifeq ($(PLATFORM), bluerock)
LIBS += intrusive
endif

$(eval $(call dep_hook,msr,$(LIBS)))
