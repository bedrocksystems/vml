LIBS = log lang cxx zeta nova pt msc uuid alloc cpu_model gic vcpu_roundup vbus simple_as $(PLATFORM)

$(eval $(call dep_hook,vmi_interface,$(LIBS)))
