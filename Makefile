#
# Makefile
#
# Copyright (C) 2020-2023 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

.DEFAULT_GOAL := all
DOXYGEN ?= doxygen
export PLATFORM ?= posix
UNAME_M = $(shell uname -m)

ifeq ($(UNAME_M),arm64)
ARCH ?= aarch64
else ifeq ($(UNAME_M),x86_64)
ARCH ?= x86_64
else
$(error Unsupported architecture $(UNAME_S))
endif

export ARCH

SUBDIRS = devices/vbus devices/vpl011 devices/gic arch/arch_api devices/timer devices/simple_as
SUBDIRS += devices/virtio_base devices/virtio_console devices/virtio_net devices/msr
SUBDIRS += vcpu/vcpu_roundup vcpu/cpu_model devices/virtio_sock
SUBDIRS += platform/posix_core

ifeq ($(ARCH), aarch64)
SUBDIRS += devices/firmware
endif

ifeq ($(CMDGOAL), doc)
check_tool=$(if $(shell which $(1)),,$(error "$(1) not found - Consider installing this tool))

doc:
		$(call check_tool,$(DOXYGEN))
		$(DOXYGEN) doc/Doxyfile
		@echo "================================================="
		@echo "Documentation generated in doc/html and doc/latex"
		@echo "================================================="
.PHONY: doc

else

export BLDDIR ?= build-$(PLATFORM)-$(ARCH)/

EXAMPLES = examples/vbus_posix examples/virtio_posix

define include_bu
$(eval BU := $(notdir $(1)))
$(eval CUR_DIR := $(1)/)
$(eval include $(1)/Makefile)
$(eval include $(1)/deps.mk)
$(eval include support/build/bu.mk)
endef

define include_bu_lib
$(eval LIBNAME := $(notdir $(1)))
$(call include_bu,$(1))
endef
$(foreach l,$(SUBDIRS), $(call include_bu_lib,$l))
ALL_LIB_OUTPUTS := $(foreach e, $(SUBDIRS), $($(notdir $e)_OUTPUT))

define include_bu_app
$(eval APPNAME := $(notdir $(1)))
$(call include_bu,$(1))
endef
undefine LIBNAME
$(foreach l,$(EXAMPLES), $(call include_bu_app,$l))
undefine APPNAME
undefine CUR_DIR

all: $(foreach e, $(EXAMPLES), $($(notdir $e)_OUTPUT))

clean:
	rm -Rf $(BLDDIR)

RUN_TEST_PREFIX=run_test_

test: $(addprefix $(RUN_TEST_PREFIX),$(EXAMPLES))
.PHONY: test

define run_example
$(RUN_TEST_PREFIX)$(1): $(1)
	$(BLDDIR)$(1)/$(notdir $(1))
endef

$(foreach e,$(EXAMPLES),$(eval $(call run_example,$e)))
endif
