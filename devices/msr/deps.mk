LIBS = vbus cpu_model timer gic simple_as $(PLATFORM) arch_api

$(eval $(call dep_hook,msr,$(LIBS)))
