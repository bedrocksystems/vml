#
# Copyright (C) 2019-2020 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

APPNAME  = virtio_posix

include deps.mk

LINKLIBS  = vbus timer gic cpu_model vcpu_roundup virtio_console virtio_net virtio_base simple_as $(ARCH)

CC_SRCS = virtio_example.cpp
OBJS = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

include $(BHV_ROOT)/support/build/rules.mk
