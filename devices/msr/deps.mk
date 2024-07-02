LIBS = vbus cpu_model timer gic simple_as arch_api
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif


$(eval $(call dep_hook,msr,$(LIBS)))
