# -*- mode:makefile -*-
#
# Make script Makefile
#
# Copyright (c) 2020 Raoul M. Gough
#
# This file is part of DRTI.
#
# DRTI is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by published by the Free Software Foundation, version 3 only.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
# History
# =======
# 2020/03/16   rmg     File creation
#

all: test

include ../drti_base.mk

OPT = -O3

DRTI_LIB = $(DRTI_BASE_DIR)passes/libdrti.so

LLVM_OPT_FLAGS += -enable-new-pm -load-pass-plugin=$(DRTI_LIB) -debug-only=drti

LLCFLAGS += -load=$(DRTI_LIB) -debug-only=drti

# Export this as an environment variable for use by the drti-decorate
# LLVM pass
export DRTI_TARGETS_FILE = drti_test_targets.txt

test: intercept_tests-drti raw_tests-drti
	./intercept_tests-drti && ./raw_tests-drti

test_target1.o: WARN += -Wno-return-stack-address
test_target1.bc: WARN += -Wno-return-stack-address

DRTI_MODULES = \
	test_target1 \
	test_target2 \
	test_target3 \
	test_target4 \
	test_class

PLAIN_MODULES = \
	test_support

raw_tests-drti: \
	raw_tests-drti.o \
	$(DRTI_MODULES:%=%-drti.o) \
	$(PLAIN_MODULES:%=%.o) \
	$(DRTI_BASE_DIR)drti/drtiruntime.so

intercept_tests.%: CXXFLAGS += -I .. -std=c++17

intercept_tests-drti: \
	intercept_tests-drti.o \
	$(DRTI_MODULES:%=%-drti.o) \
	$(PLAIN_MODULES:%=%.o)

%-drti.bc: %.bc $(DRTI_LIB) $(DRTI_TARGETS_FILE)
	$(LLVM_OPT) $(LLVM_OPT_FLAGS) $(OPT) -o $@ $<

CLEANABLE += raw_tests-drti intercept_tests-drti

include ../drti_end.mk

# Copies of the .o dependencies for .bc targets
-include $(depfiles:.d=.bcd)
