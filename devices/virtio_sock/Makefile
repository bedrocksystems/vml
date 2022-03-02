#
# Makefile
#
# Copyright (C) 2021 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

PLATFORM ?= bedrock
BHV_ROOT ?= ../../../../..
LIBNAME = $(shell basename `pwd`)

ifeq ($(PLATFORM), bedrock)
PLATFORM_PATH ?= ../../../lib/
else
PLATFORM_PATH ?= ../../platform/
endif

LIBDIR  = ../ ../../config/ ../../vcpu/ $(PLATFORM_PATH)

include deps.mk

CC_SRCS = virtio_sock.cpp
OBJS    = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

include $(BHV_ROOT)/support/build/rules.mk
