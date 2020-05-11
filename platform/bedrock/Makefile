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
LIBDIR  = $(ZT_ROOT)lib/ ../../devices/ ../../config/ ../../vcpu/ ../../interfaces/

ENABLE_VMI ?= 0
ifneq ($(ENABLE_VMI), 0)
LIBDIR += ../../../../../lib/ 
endif

include deps.mk

CC_SRCS = vswitch_interface.cpp ec.cpp umx_interface.cpp portal.cpp vmexit.cpp fdt.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

SPECS =
PROOFS =

include $(ZT_ROOT)support/build/rules.mk
