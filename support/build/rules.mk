#
# Makefile's rules
#
# Copyright (C) 2020 BedRock Systems, Inc.
# All rights reserved.
#
# This software is distributed under the terms of the BedRock Open-Source License.
# See the LICENSE-BedRock file in the repository root for details.
#
# At the moment, this Makefile is allowing us to compile the code of VMM that is independent
# from Zeta. It is not meant to build a fully functionnal application. For convenience, it is
# targeting x86 by default. That can be changed by tweaking the options below.
#

# Define the set of default variables to build LibVMM



UNAME_S = $(shell uname -s | tr A-Z a-z)
PLATFORM ?= posix
LLVM ?= 1
CXX ?= clang++
AR ?= llvm-ar
BLDDIR ?= build/posix-$(PLATFORM)-$(ARCH)/
MFLAGS = -MP -MMD -pipe
CXXVERSION ?= -std=gnu++17
FFLAGS = -fdata-sections -ffunction-sections -fomit-frame-pointer -fno-asynchronous-unwind-tables \
-fno-stack-protector -fvisibility=hidden -fvisibility-inlines-hidden
FFLAGS += $(CXXVERSION)
DFLAGS ?=
WFLAGS = -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion \
-Wdisabled-optimization -Wformat=2 -Wmissing-format-attribute \
-Wmissing-noreturn -Wold-style-cast -Woverloaded-virtual -Wpacked -Wpointer-arith \
-Wredundant-decls -Wshadow -Wsign-promo -Wstrict-overflow=5 \
-Wwrite-strings -Wzero-as-null-pointer-constant
XFLAGS = -fno-exceptions -fno-rtti -fno-threadsafe-statics

ifeq ($(ARCH), aarch64)
AFLAGS ?= -march=armv8-a -mgeneral-regs-only -mstrict-align
endif

ifeq ($(DEBUG), 1)
OFLAGS = -g
else
OFLAGS = -O2
endif

find_common_path=$(patsubst $(1)%,%, $(2))
PATH_TO_OBJS = $(call find_common_path,$(VMM_ROOT),$(realpath $(CURDIR)))

SRCDIR = src/
OBJDIR = $(BLDDIR)$(PATH_TO_OBJS)/
ifeq ($(ARCH), x86_64)
ARCH_INC = x86
else
ARCH_INC = $(ARCH)
endif

#
# We keep things simple in the posix version for now. We just flatten all
# the libraries and libdir into one namespace. A better version would be too keep
# track of dependencies and traverse them using the "deps.mk" files.
# For now, this is not needed, we just want to compile examples.
#

LIBDIR := $(VMM_ROOT)devices $(VMM_ROOT)arch $(VMM_ROOT)vcpu $(VMM_ROOT)config $(VMM_ROOT)platform
LIBS  = vbus vpl011 gic irq_controller vuart timer arch_api simple_as virtio_base virtio_console
LIBS += virtio_net firmware vcpu_roundup cpu_model virtio_sock vmm_debug posix posix_core lifecycle msr

find_path_to_lib = $(foreach d, $(LIBDIR), $(wildcard $(d)/$(1)))
INCLS := $(foreach l, $(LIBS), $(addsuffix /include,$(call find_path_to_lib,$l)))
INCLS += $(foreach l, $(LIBS), $(addsuffix /include/$(ARCH_INC),$(call find_path_to_lib,$l)))

APPINCL := ./include ./include/$(ARCH_INC)
IFLAGS := $(addprefix -I, $(APPINCL)) $(addprefix -I, $(INCLS))
LINKDEPS := $(dir $(ALL_LIB_OUTPUTS))
EXTRA_LINK = -pthread

ifeq ($(UNAME_S), linux)
EXTRA_LINK += -lrt
endif

CXXFLAGS := $(TARGET_FLAG) $(MFLAGS) $(AFLAGS) $(OFLAGS) $(FFLAGS) $(XFLAGS) $(IFLAGS) $(DFLAGS) $(WFLAGS)

OBJS := $(addprefix $($(BU)_OBJDIR), $(patsubst %.cpp,%.o, $($(BU)_SRCS)))
DEPS := $(patsubst %.o,%.d, $(OBJS))

$($(BU)_OBJDIR):
	@mkdir -p $@ 2>&1 > /dev/null

$($(BU)_OBJDIR)%.o: $($(BU)_SRCDIR)%.cpp $(MAKEFILE_LIST) | $($(BU)_OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$($(BU)_OBJDIR)%.o: $($(BU)_SRCDIR)/$(ARCH_INC)/%.cpp $(MAKEFILE_LIST)  | $($(BU)_OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$($(BU)_OBJDIR)%.o: CXXFLAGS := $(CXXFLAGS)

ifdef LIBNAME
$($(BU)_OBJDIR)lib$(LIBNAME).a: $(OBJS)
	$(AR) rcs $@ $^

else ifdef APPNAME

$($(BU)_OBJDIR)$(APPNAME): $(OBJS) $(ALL_LIB_OUTPUTS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(addprefix -L, $(dir $(LINKDEPS))) $(patsubst %,-l%,$(LINKLIBS)) $(EXTRA_LINK)
$($(BU)_OBJDIR)$(APPNAME): OBJS := $(OBJS)
$($(BU)_OBJDIR)$(APPNAME): LINKDEPS := $(LINKDEPS)
$($(BU)_OBJDIR)$(APPNAME): LINKLIBS := $(LINKLIBS)
$($(BU)_OBJDIR)$(APPNAME): EXTRA_LINK := $(EXTRA_LINK)
else
$(error "Not sure what to build...")
endif

-include $(DEPS)
