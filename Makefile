#
# Makefile
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

BHV_ROOT ?= ../../..
DOXYGEN ?= doxygen
CMDGOAL = $(if $(strip $(MAKECMDGOALS)),$(MAKECMDGOALS),all)
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
SUBDIRS += devices/firmware vcpu/vcpu_roundup vcpu/cpu_model devices/virtio_sock


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
ifeq ($(PLATFORM), posix)
SUBDIRS += platform/posix_core

export BLDDIR ?= $(CURDIR)/build-$(PLATFORM)-$(ARCH)/

EXAMPLES = examples/vbus_posix examples/virtio_posix

$(CMDGOAL): $(EXAMPLES)

ifeq ($(CMDGOAL),test)
SUBCMDGOAL=all
else
SUBCMDGOAL=$(CMDGOAL)
endif

$(EXAMPLES) $(SUBDIRS):
		+$(MAKE) -C $@ BHV_ROOT=$(realpath .)/ VMM_ROOT=$(realpath .)/ $(SUBCMDGOAL)

$(EXAMPLES): $(SUBDIRS)

RUN_TEST_PREFIX=run_test_

test: $(addprefix $(RUN_TEST_PREFIX),$(EXAMPLES))
.PHONY: test

define run_example
$(RUN_TEST_PREFIX)$(1): $(1)
	$(BLDDIR)$(1)/$(notdir $(1))
endef

$(foreach e,$(EXAMPLES),$(eval $(call run_example,$e)))

else
# Bedrock platform
$(CMDGOAL): $(SUBDIRS)
$(SUBDIRS):
		+$(MAKE) -C $@ $(CMDGOAL)
endif
endif

.PHONY: $(CMDGOAL) $(EXAMPLES) $(SUBDIRS)
