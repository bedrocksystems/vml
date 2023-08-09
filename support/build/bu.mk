#
# Makefile
#
# Copyright (C) 2023 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#

$(BU)_DIR := $(CUR_DIR)
$(BU)_SRCDIR := $(CUR_DIR)src/
$(BU)_OBJDIR := $(BLDDIR)$(CUR_DIR)
$(BU)_INCDIR := $(CUR_DIR)include/
$(BU)_SRCS := $(CC_SRCS)
$(BU)_DIR  := $(CUR_DIR)

ifneq ($(APPNAME),)
$(BU)_OUTPUT := $($(BU)_OBJDIR)$(APPNAME)
else
$(BU)_OUTPUT := $($(BU)_OBJDIR)lib$(LIBNAME).a
endif

include support/build/rules.mk
