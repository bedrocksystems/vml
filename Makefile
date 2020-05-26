#
# Makefile
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
# see repository root for details.
#

BRS_ROOT ?= ../../zeta/
DOXYGEN ?= doxygen
PLATFORM ?= posix
CMDGOAL = $(if $(strip $(MAKECMDGOALS)),$(MAKECMDGOALS),all)

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

TARGET ?= x86_64
SUBDIRS = devices/vbus devices/pl011 devices/gic arch/$(TARGET)
SUBDIRS += devices/timer devices/virtio_console devices/virtio_net devices/msr
SUBDIRS += devices/simple_as devices/firmware vcpu/vcpu_roundup vcpu/cpu_model

EXAMPLES = examples/vbus_posix examples/virtio_posix

$(CMDGOAL): $(EXAMPLES)
.PHONY: $(CMDGOAL) $(EXAMPLES) $(SUBDIRS)

$(EXAMPLES) $(SUBDIRS):
		+$(MAKE) -C $@ BRS_ROOT=$(realpath .)/ VMM_ROOT=$(realpath .)/ TARGET=$(TARGET) PLATFORM=posix $(CMDGOAL)

$(EXAMPLES): $(SUBDIRS)

else
$(error "PLATFORM: $(PLATFORM) is invalid. This makefile is meant to be used for POSIX compilation only for now")
endif
endif
