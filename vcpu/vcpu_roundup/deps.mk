LIBS = cpu_model $(PLATFORM)

$(eval $(call dep_hook,vcpu_roundup,$(LIBS)))
