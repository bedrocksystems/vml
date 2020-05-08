#
# Makefile
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

PLATFORM ?= bedrock
BRS_ROOT ?= ../../../../zeta/
LIBNAME = $(shell basename `pwd`)

ifeq ($(PLATFORM), bedrock)
PLATFORM_PATH ?= ../../../../apps/vmm/lib/
else
PLATFORM_PATH ?= ../../platform/
endif

LIBDIR  = $(BRS_ROOT)lib/  ../ ../../config/ ../../vcpu/ ../../arch/ $(PLATFORM_PATH)

include deps.mk

CC_SRCS = pl011.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

SPECS =
PROOFS =

include $(BRS_ROOT)support/build/rules.mk
