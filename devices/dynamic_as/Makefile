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
LIBDIR  = $(BRS_ROOT)lib/  ../ ../../arch/ ../../platform/ ../../vcpu/ ../../config/ ../../interfaces/ ../../devices/

ENABLE_VMI ?= 0
ifneq ($(ENABLE_VMI), 0)
LIBDIR += ../../../../lib/
endif

include deps.mk

CC_SRCS = guest_as.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

SPECS =
PROOFS =

include $(BRS_ROOT)support/build/rules.mk
