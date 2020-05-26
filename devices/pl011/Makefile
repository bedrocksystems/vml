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
LIBDIR  = $(BRS_ROOT)lib/  ../ ../../platform/ ../../config/ ../../vcpu/ ../../arch/

include deps.mk

CC_SRCS = pl011.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

SPECS =
PROOFS =

include $(BRS_ROOT)support/build/rules.mk
