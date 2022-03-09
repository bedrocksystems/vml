LIBS = vbus irq_controller cpu_model $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log zeta nova pt alloc msc uuid concurrent
endif

$(eval $(call dep_hook,timer,$(LIBS)))
