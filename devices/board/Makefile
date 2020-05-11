#
# Makefile
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

ZT_ROOT ?= ../../../../zeta/
LIBNAME = $(shell basename `pwd`)
LIBDIR  = $(ZT_ROOT)lib/ ../ ../../vcpu/ ../../config/ ../../interfaces/ ../../platform/

LIB_PM  ?= ../../../pm/
LIBDIR += $(LIB_PM)

include deps.mk

CC_SRCS = board.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

SPECS =
PROOFS =

include $(ZT_ROOT)support/build/rules.mk
