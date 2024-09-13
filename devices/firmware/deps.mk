LIBS = cpu_model msr vbus $(PLATFORM)

ifeq ($(PLATFORM), posix)
# Empty lifecycle, just provides the API
LIBS += lifecycle
else
LIBS += lifecycle_bluerock
endif

$(eval $(call dep_hook,firmware,$(LIBS)))
