# platform
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
else
LIBS += platform_wrapper
endif


ifeq ($(ARCH), aarch64)
LIBS += cpu_model msr
endif

$(eval $(call dep_hook,arch_api,$(LIBS)))
