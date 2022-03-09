LIBS = arch_api $(PLATFORM)

$(eval $(call dep_hook,x86_64,$(LIBS)))
