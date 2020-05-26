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
LIBDIR  = $(BRS_ROOT)lib/  ../ ../../devices/ ../../interfaces/ ../../config/ ../../platform/
LIBDIR += ../../arch/
ENABLE_VMI ?= 0

# XXX: this will go away in profit of a lib in interfaces for PM (similar to VMI)
LIB_PM  ?= ../../../../../lib/pm/
LIBDIR += $(LIB_PM)

ifneq ($(ENABLE_VMI), 0)
LIBDIR += ../../../../lib/
endif

include deps.mk

CC_SRCS = vcpu.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

SPECS =
PROOFS =

include $(BRS_ROOT)support/build/rules.mk
