ifeq ($(HOSTED), 1)
LIBS = posix_host
endif

$(eval $(call dep_hook,posix,$(LIBS)))
