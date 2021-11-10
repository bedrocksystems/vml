LIBS = vbus arch_api $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log zeta alloc nova msc pt uuid
endif

$(eval $(call dep_hook,simple_as,$(LIBS)))
