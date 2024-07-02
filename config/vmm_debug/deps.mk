ifneq ($(HOSTED), 1)
LIBS = $(PLATFORM)
else
LIBS = platform_wrapper
endif


$(eval $(call dep_hook,vmm_debug,$(LIBS)))
