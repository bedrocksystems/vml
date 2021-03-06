LIBS = arch_api msr cpu_model $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang
LIBS += zeta cxx nova pt alloc msc uuid log concurrent # deps of cpu_model
endif

$(eval $(call dep_hook,aarch64,$(LIBS)))
