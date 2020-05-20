#
# Copyright (C) 2019-2020 BedRock Systems, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#

TARGET ?= x86_64
PLATFORM = posix
BRS_ROOT ?= ../../../../zeta/

APPNAME  = virtio_example

include deps.mk

LINKLIBS = vbus timer cpu_model vcpu_roundup gic virtio_console virtio_net $(TARGET)

LIBDIR  = ../../devices/  ../../vcpu/ ../../config/ ../../platform/ ../../arch/

CC_SRCS = virtio_example.cpp
OBJS = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

include $(BRS_ROOT)support/build/rules.mk
