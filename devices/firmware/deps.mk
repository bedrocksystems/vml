LIBS = cpu_model msr vbus $(PLATFORM)

ifeq ($(HOSTED), 1)
# Empty lifecycle, just provides the API
LIBS += lifecycle
else
LIBS += lifecycle_bluerock
endif

$(eval $(call dep_hook,firmware,$(LIBS)))
