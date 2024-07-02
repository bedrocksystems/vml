LIBS = vbus
ifneq ($(HOSTED), 1)
LIBS += $(PLATFORM)
endif


$(eval $(call dep_hook,vuart,$(LIBS)))
