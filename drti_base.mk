# -*- mode:makefile -*-
#
# Make include file drti_base.mk
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
# 2019/11/06   rmg     File creation from lto_example/Makefile.inc
#

DRTI_BASE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

LLVM_EXE_ROOT_DIR ?= ~/install/llvm-9.0.1
LLVM_LIB_ROOT_DIR ?= $(LLVM_EXE_ROOT_DIR)

LLVM_EXE_BIN_DIR = $(LLVM_EXE_ROOT_DIR)/bin
LLVM_LIB_BIN_DIR = $(LLVM_LIB_ROOT_DIR)/bin

LLVM_CONFIG = $(LLVM_LIB_BIN_DIR)/llvm-config

LLVM_EXE_CLANG = $(LLVM_EXE_BIN_DIR)/clang++
LLVM_EXE_DIS = $(LLVM_EXE_BIN_DIR)/llvm-dis
LLVM_EXE_LINK = $(LLVM_EXE_BIN_DIR)/llvm-link
LLVM_EXE_LLC = $(LLVM_EXE_BIN_DIR)/llc
LLVM_EXE_OPT = $(LLVM_EXE_BIN_DIR)/opt

LLVM_LIB_CLANG = $(LLVM_LIB_BIN_DIR)/clang++
LLVM_LIB_DIS = $(LLVM_LIB_BIN_DIR)/llvm-dis
LLVM_LIB_LINK = $(LLVM_LIB_BIN_DIR)/llvm-link
LLVM_LIB_LLC = $(LLVM_LIB_BIN_DIR)/llc
LLVM_LIB_OPT = $(LLVM_LIB_BIN_DIR)/opt

CLANG = $(LLVM_EXE_CLANG)
# Need -load compatibility
LLC = $(LLVM_LIB_LLC)
LLVM_DIS = $(LLVM_EXE_DIS)
LLVM_LINK = $(LLVM_EXE_LINK)
# Need -load compatibility
LLVM_OPT = $(LLVM_LIB_OPT)

CXX = $(CLANG)
PIC = -fPIC
# OPT =
DEBUG = -g
LTO =
WARN = -Wall -Werror
DEP = -MMD
LLCFLAGS = --relocation-model=pic

CXXFLAGS = $(DEP) $(INCLUDES) $(PIC) $(OPT) $(WARN) $(DEBUG) $(LTO)
LDFLAGS_SHARED = -Wl,-zdefs
LINK.o = $(LINK.cc)

CLEANABLE = core.* *.d *.bcd *.o *.so *.a *.ii *.ll *.bc perf.data callgrind.* perf.*

%.bc: %.cpp
	$(CLANG) $(CXXFLAGS) -emit-llvm -o $@ -c $<

# Duplicate the .o dependencies for .bc targets
%.bcd: %.d
	sed 's/[.]o:/.bc:/' $< >$@

%.o: %.bc
	$(LLC) $(LLCFLAGS) -filetype=obj -o $@ $<

%.s: %.bc
	$(LLC) $(LLCFLAGS) -filetype=asm -o $@ $<

%.o: %.ll
	$(LLC) $(LLCFLAGS) -filetype=obj -o $@ $<

%.s: %.ll
	$(LLC) $(LLCFLAGS) -filetype=asm -o $@ $<

%.s: %.bc
	$(LLC) $(LLCFLAGS) --asm-show-inst -filetype=asm -o $@ $<

%.s: %.ll
	$(LLC) $(LLCFLAGS) --asm-show-inst -filetype=asm -o $@ $<

%.ll: %.bc
	$(LLVM_DIS) -o $@ $<

lib%.so: %.o
	$(LINK.o) $(LDFLAGS_SHARED) $^ $(LOADLIBES) $(LDLIBS) -shared -o $@

.PRECIOUS: %.o
