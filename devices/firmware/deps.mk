LIBS = cpu_model simple_as vcpu_roundup msr $(PLATFORM)

ifeq ($(HOSTED), 1)
# Empty lifecycle, just provides the API
LIBS += lifecycle
else
LIBS += lifecycle_bluerock
endif

$(eval $(call dep_hook,firmware,$(LIBS)))
