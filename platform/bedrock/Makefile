#
# Makefile
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
# see repository root for details.
#

BRS_ROOT ?= ../../../../zeta/
LIBNAME = $(shell basename `pwd`)
LIBDIR  = $(BRS_ROOT)lib/ ../../devices/ ../../config/ ../../vcpu/ ../../interfaces/

ENABLE_VMI ?= 0
ifneq ($(ENABLE_VMI), 0)
LIBDIR += ../../../../lib/
endif

include deps.mk

CC_SRCS = vswitch_interface.cpp umx_interface.cpp portal.cpp vmexit.cpp fdt.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

SPECS =
PROOFS =

include $(BRS_ROOT)support/build/rules.mk
