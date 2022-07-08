LIBS = arch_api msr cpu_model $(PLATFORM)

$(eval $(call dep_hook,aarch64,$(LIBS)))
