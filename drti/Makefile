# -*- mode:makefile -*-
#
# Make script Makefile for drti/runtime
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
# 2019/11/25   rmg     File creation
#

include ../llvm_libs.mk

INCLUDES = -I ..

CXXFLAGS += -fvisibility=hidden

all: drtiruntime.so libdrti-common.a

libdrti-common.a: libdrti-common.a(drti-common.o)

drtiruntime.so: runtime.o libdrti-common.a
	$(LINK.o) $(LDFLAGS_SHARED) $^ $(LOADLIBES) $(LDLIBS) -shared -o $@

include ../drti_end.mk
