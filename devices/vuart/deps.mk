LIBS = vbus $(PLATFORM)

$(eval $(call dep_hook,vuart,$(LIBS)))
