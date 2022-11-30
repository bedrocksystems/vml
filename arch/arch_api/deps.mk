# platform
LIBS += $(PLATFORM)

ifeq ($(ARCH), aarch64)
LIBS += cpu_model msr
endif

$(eval $(call dep_hook,arch_api,$(LIBS)))
