#
# Makefile
#
# Copyright (C) 2020-2022 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

PLATFORM ?= bedrock
BHV_ROOT ?= ../../../../..
LIBNAME = $(shell basename `pwd`)

include deps.mk

CC_SRCS = timer.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

include $(BHV_ROOT)/support/build/rules.mk
