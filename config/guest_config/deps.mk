LIBS = lang cxx log

$(eval $(call dep_hook,guest_config,$(LIBS)))
