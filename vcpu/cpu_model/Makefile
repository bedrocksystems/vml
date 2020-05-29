#
# Makefile
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
# see repository root for details.
#

PLATFORM ?= bedrock
BRS_ROOT ?= ../../../../zeta/
LIBNAME = $(shell basename `pwd`)

ifeq ($(PLATFORM), bedrock)
PLATFORM_PATH ?= ../../../../apps/vmm/lib/
else
PLATFORM_PATH ?= ../../platform/
endif

LIBDIR  = $(BRS_ROOT)lib/  ../../devices/ ../../config/ ../../arch/ ../ $(PLATFORM_PATH)

include deps.mk

CC_SRCS = cpu_model.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

SPECS =
PROOFS =

include $(BRS_ROOT)support/build/rules.mk
