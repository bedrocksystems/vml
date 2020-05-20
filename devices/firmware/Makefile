#
# Makefile
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

PLATFORM ?= bedrock
BRS_ROOT ?= ../../../../zeta/
LIBNAME = $(shell basename `pwd`)
LIBDIR  = $(BRS_ROOT)lib/  ../ ../../vcpu/ ../../config/ ../../platform/

ifeq ($(PLATFORM), bedrock)
LIB_PM  ?= ../../../pm/
LIBDIR += $(LIB_PM)
endif

include deps.mk

CC_SRCS = psci.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

ifeq ($(PLATFORM), bedrock)
CC_SRCS += platform_device.cpp platform_firmware.cpp
endif

SPECS =
PROOFS =

include $(BRS_ROOT)support/build/rules.mk
