LIBS = cpu_model $(PLATFORM) arch_api

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx uuid msc zeta nova alloc pt concurrent log
endif

$(eval $(call dep_hook,vcpu_roundup,$(LIBS)))
