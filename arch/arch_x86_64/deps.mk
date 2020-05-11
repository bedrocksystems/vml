LIBS = arch_api $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang
endif

$(eval $(call dep_hook,arch_x86_64,$(LIBS)))
