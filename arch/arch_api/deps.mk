# platform
LIBS += $(PLATFORM)

$(eval $(call dep_hook,arch_api,$(LIBS)))
