#
# Makefile
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

PLATFORM ?= bedrock
ZT_ROOT ?= ../../../../zeta/
LIBNAME = $(shell basename `pwd`)
LIBDIR = $(ZT_ROOT)lib/ ../../platform/

include deps.mk

CC_SRCS = vbus.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

SPECS =
PROOFS =

include $(ZT_ROOT)support/build/rules.mk
