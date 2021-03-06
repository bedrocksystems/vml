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

ARCH ?= x86_64
PLATFORM ?= posix
LLVM ?= 1
CXX ?= clang++
AR ?= llvm-ar
BLDDIR ?= build/posix-$(PLATFORM)-$(ARCH)/
MFLAGS = -MP -MMD -pipe
CXXVERSION ?= -std=gnu++17
FFLAGS = -funit-at-a-time \
-fdata-sections -ffunction-sections -fomit-frame-pointer -fno-asynchronous-unwind-tables \
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
ifeq ($(LLVM), 1)
TARGET_FLAG = --target=$(ARCH)-none-elf
endif

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

find_path_to_lib = $(foreach d, $(LIBDIR), $(wildcard $(d)$(1)))
find_path_to_lib_objs=$(call find_common_path,$(VMM_ROOT),$(realpath $(call find_path_to_lib,$(1))))
find_path_to_archive=$(BLDDIR)$(call find_path_to_lib_objs,$(1))/lib$(1).a
INCLS  = $(foreach l, $(LIBS), $(addsuffix /include,$(call find_path_to_lib,$l)))
INCLS += $(foreach l, $(LIBS), $(addsuffix /include/$(ARCH_INC),$(call find_path_to_lib,$l)))
APPINCL = ./include ./include/$(ARCH_INC)
IFLAGS = $(addprefix -I, $(APPINCL)) $(addprefix -I, $(INCLS))
LINKDEPS = $(foreach l,$(LINKLIBS),$(call find_path_to_archive,$l))
EXTRA_LINK = -pthread

CXXFLAGS ?= $(TARGET_FLAG) $(MFLAGS) $(AFLAGS) $(OFLAGS) $(FFLAGS) $(XFLAGS) $(IFLAGS) $(DFLAGS) $(WFLAGS)

DEPS = $(patsubst %.o,%.d, $(OBJS))


$(OBJDIR)%.o: $(SRCDIR)%.cpp
	@mkdir -p $(@D) 2>&1 > /dev/null
	$(CXX) $(CXXFLAGS) -c -o $@ $<

ifdef LIBNAME
all: $(OBJDIR)lib$(LIBNAME).a

$(OBJDIR)lib$(LIBNAME).a: $(OBJS)
	$(AR) rcs $@ $^

else ifdef APPNAME

all: $(OBJDIR)$(APPNAME)

$(OBJDIR)$(APPNAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(addprefix -L, $(dir $(LINKDEPS))) $(patsubst %,-l%,$(LINKLIBS)) $(EXTRA_LINK)
else

$(error "Not sure what to build...")

endif

clean:
	rm -rf $(BLDDIR)

.PHONY: all clean

-include $(DEPS)
