#
# Copyright (C) 2019-2020 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

TARGET ?= x86_64
PLATFORM = posix
BRS_ROOT ?= ../../../../zeta/

APPNAME  = vbus_example

include deps.mk

LINKLIBS  = vbus simple_as firmware pl011 msr timer gic cpu_model vcpu_roundup virtio_console virtio_net simple_as $(ARCH)

LIBDIR  = ../../devices/  ../../vcpu/ ../../config/ ../../platform/ ../../arch/

CC_SRCS = vbus_example.cpp
OBJS = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

include $(BRS_ROOT)support/build/rules.mk
