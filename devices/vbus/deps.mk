LIBS = vmm_debug $(PLATFORM)

$(eval $(call dep_hook,vbus,$(LIBS)))
