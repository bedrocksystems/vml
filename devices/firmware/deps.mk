LIBS = cpu_model simple_as vcpu_roundup msr

ifeq ($(PLATFORM), bedrock)
LIBS += lifecycle_bedrock bedrock
else
# Empty lifecycle, just provides the API
LIBS += lifecycle
endif

$(eval $(call dep_hook,firmware,$(LIBS)))
