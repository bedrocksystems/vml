LIBS = $(PLATFORM) vmm_debug

$(eval $(call dep_hook,vbus,$(LIBS)))
