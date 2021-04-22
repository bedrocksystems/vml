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
export TARGET ?= x86_64

SUBDIRS = devices/vbus devices/pl011 devices/gic arch/$(TARGET)
SUBDIRS += devices/timer devices/virtio_console devices/virtio_net devices/msr
SUBDIRS += devices/simple_as devices/firmware vcpu/vcpu_roundup vcpu/cpu_model
SUBDIRS += devices/virtio_sock

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

export BLDDIR ?= $(CURDIR)/build-$(PLATFORM)-$(TARGET)/

EXAMPLES = examples/vbus_posix examples/virtio_posix

$(CMDGOAL): $(EXAMPLES)

$(EXAMPLES) $(SUBDIRS):
		+$(MAKE) -C $@ BRS_ROOT=$(realpath .)/ VMM_ROOT=$(realpath .)/ $(CMDGOAL)

$(EXAMPLES): $(SUBDIRS)

else
# Bedrock platform
$(CMDGOAL): $(SUBDIRS)
$(SUBDIRS):
		+$(MAKE) -C $@ $(CMDGOAL)
endif
endif

.PHONY: $(CMDGOAL) $(EXAMPLES) $(SUBDIRS)
