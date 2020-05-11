LIBS = arch_api msr $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang
endif

$(eval $(call dep_hook,arch_aarch64,$(LIBS)))
