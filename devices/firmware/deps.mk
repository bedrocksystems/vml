LIBS = cpu_model simple_as vcpu_roundup msr

ifeq ($(PLATFORM), bluerock)
LIBS += lifecycle_bluerock bluerock
else
# Empty lifecycle, just provides the API
LIBS += lifecycle
endif

$(eval $(call dep_hook,firmware,$(LIBS)))
