LIBS = vbus arch_api $(PLATFORM)

$(eval $(call dep_hook,simple_as,$(LIBS)))
