LIBS = vbus cpu_model $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx
endif

$(eval $(call dep_hook,irq_controller,$(LIBS)))
