LIBS = $(PLATFORM)

ifeq ($(PLATFORM), bedrock)
LIBS += lang cxx log
endif

$(eval $(call dep_hook,vbus,$(LIBS)))
