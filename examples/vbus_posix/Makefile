#
# Copyright (C) 2019-2022 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

APPNAME  = vbus_posix

include deps.mk

LINKLIBS  = vbus simple_as arch_api vpl011 gic msr timer cpu_model vcpu_roundup virtio_console virtio_net posix_core

CC_SRCS = vbus_example.cpp
OBJS = $(CC_SRCS:%.cpp=$(OBJDIR)%.o)

include $(BHV_ROOT)/support/build/rules.mk
