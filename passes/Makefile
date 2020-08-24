# -*- mode:makefile -*-
#
# Make script Makefile
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
# 2019/11/06   rmg     File creation
#

include ../llvm_libs.mk

LDFLAGS += -L ../drti

INCLUDES += -I ..

all: libdrti.so

# OPT = -O1

# We don't want debug info bloating our inline bitcode
drti-inline.bc: DEBUG =
drti-inline.bc: OPT ?= -O1

%-bc.o: %.bc
	objcopy --input binary -B i386 --output elf64-x86-64 $< $@

libdrti.so: drti-decorate.o drti-target.o drti-inline-bc.o ../drti/libdrti-common.a
	$(LINK.o) $(LDFLAGS_SHARED) $^ $(LOADLIBES) $(LDLIBS) -shared -o $@

include ../drti_end.mk
