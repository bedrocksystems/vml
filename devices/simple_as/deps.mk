LIBS = vbus arch_api
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif


$(eval $(call dep_hook,simple_as,$(LIBS)))
