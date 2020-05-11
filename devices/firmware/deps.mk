LIBS = vbus cpu_model gic timer vmm_debug simple_as vcpu vcpu_roundup $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
# deps of PM
LIBS += nova zeta pt msc uuid alloc lang cxx log pm_client
endif

$(eval $(call dep_hook,firmware,$(LIBS)))
