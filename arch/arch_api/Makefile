#
# Copyright (C) 2019-2022 BedRock Systems, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: LGPL-2.1 WITH BedRock Exception for use over network,
# see repository root for details.
#

# If we are building in the context of BHV, which we probably are,
# just consume the BHV config. This allows a local invocation of make
# to respect the global parameters set in bhv.
BHV_ROOT ?= ../..
-include $(BHV_ROOT)/conf.mk

ARCH ?= aarch64
ENABLE_VMI ?= 0

include deps.mk


include $(BHV_ROOT)/support/build/rules.mk
