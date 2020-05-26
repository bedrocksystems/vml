#
# Copyright (C) 2019-2020 BedRock Systems, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
# see repository root for details.
#

TARGET ?= x86_64
PLATFORM = posix
BRS_ROOT ?= ../../../../zeta/

APPNAME  = vbus_example

include deps.mk

LINKLIBS = vbus simple_as firmware pl011 msr timer cpu_model vcpu_roundup gic virtio_console virtio_net $(TARGET)

LIBDIR  = ../../devices/  ../../vcpu/ ../../config/ ../../platform/ ../../arch/

CC_SRCS = vbus_example.cpp
OBJS = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

include $(BRS_ROOT)support/build/rules.mk
