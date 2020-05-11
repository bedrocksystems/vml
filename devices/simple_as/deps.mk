LIBS = vbus arch_api $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log
endif

$(eval $(call dep_hook,simple_as,$(LIBS)))
