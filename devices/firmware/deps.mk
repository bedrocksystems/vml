LIBS = vbus cpu_model gic timer vmm_debug simple_as vcpu vcpu_roundup msr $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += nova zeta pt msc uuid alloc lang cxx log pm_client lifecycle_bedrock concurrent
else
# Empty lifecycle, just provides the API
LIBS += lifecycle
endif

$(eval $(call dep_hook,firmware,$(LIBS)))
