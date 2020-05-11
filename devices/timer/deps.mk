LIBS = lang cxx vbus gic cpu_model vmm_debug $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log zeta nova pt alloc msc uuid
endif

$(eval $(call dep_hook,timer,$(LIBS)))
