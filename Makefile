#
# Makefile
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

BRS_ROOT ?= ../../zeta/
DOXYGEN ?= doxygen
CMDGOAL = $(if $(strip $(MAKECMDGOALS)),$(MAKECMDGOALS),all)
export PLATFORM ?= posix

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

export TARGET ?= x86_64
export BLDDIR ?= $(CURDIR)/build-$(PLATFORM)-$(TARGET)/
SUBDIRS = devices/vbus devices/pl011 devices/gic arch/$(TARGET)
SUBDIRS += devices/timer devices/virtio_console devices/virtio_net devices/msr
SUBDIRS += devices/simple_as devices/firmware vcpu/vcpu_roundup vcpu/cpu_model

EXAMPLES = examples/vbus_posix examples/virtio_posix

$(CMDGOAL): $(EXAMPLES)
.PHONY: $(CMDGOAL) $(EXAMPLES) $(SUBDIRS)

$(EXAMPLES) $(SUBDIRS):
		+$(MAKE) -C $@ BRS_ROOT=$(realpath .)/ VMM_ROOT=$(realpath .)/ $(CMDGOAL)

$(EXAMPLES): $(SUBDIRS)

else
$(error "PLATFORM: $(PLATFORM) is invalid. This makefile is meant to be used for POSIX compilation only for now")
endif
endif
