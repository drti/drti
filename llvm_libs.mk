# -*- mode:makefile -*-
#
# Make include file llvm_libs.mk
#
# Copyright (c) 2019, 2020 Raoul M. Gough
#
# This file is part of DRTI.
#
# DRTI is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, version 3 only.
#
# DRTI is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# History
# =======
# 2019/11/11   rmg     File creation from drti_base.mk
#

include $(dir $(lastword $(MAKEFILE_LIST)))/drti_base.mk

# Flip -std=c++14 to -std=c++17 because it's 2020 now
LLVM_CXX_FLAGS := $(patsubst %c++14,%c++17,$(shell $(LLVM_CONFIG) --cxxflags))
# We use exceptions
LLVM_CXX_FLAGS := $(filter-out -fno-exceptions,$(LLVM_CXX_FLAGS))

LLVM_LD_FLAGS := $(shell $(LLVM_CONFIG) --ldflags)
LLVM_LDLIBS := $(shell $(LLVM_CONFIG) --libs --system-libs)

# Strip out libs that interfere with pass loading into opt
LLVM_LDLIBS := $(filter-out %TableGen,$(LLVM_LDLIBS))

ifeq ($(shell $(LLVM_CONFIG) --shared-mode),shared)
	LLVM_LD_FLAGS += -Wl,--enable-new-dtags,--rpath="$(shell $(LLVM_CONFIG) --libdir)"
endif

CXXFLAGS += $(LLVM_CXX_FLAGS)
LDFLAGS += $(LLVM_LD_FLAGS)
LDLIBS += $(LLVM_LDLIBS)
