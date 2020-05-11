LIBS = lang cxx zeta nova pt msc uuid alloc cpu_model gic vcpu_roundup $(PLATFORM)

$(eval $(call dep_hook,vmi_interface,$(LIBS)))
