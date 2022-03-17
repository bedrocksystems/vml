LIBS = vbus cpu_model timer vmm_debug simple_as vcpu_roundup msr $(PLATFORM) irq_controller

ifeq ($(PLATFORM), bedrock)
LIBS += nova zeta pt msc uuid alloc lang cxx log lifecycle_bedrock concurrent
else
# Empty lifecycle, just provides the API
LIBS += lifecycle
endif

$(eval $(call dep_hook,firmware,$(LIBS)))
