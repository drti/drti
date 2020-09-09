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
# 2019/11/25   rmg     File creation
#

all:
	$(MAKE) -C drti
	$(MAKE) -C passes
	$(MAKE) -C tests

clean:
	$(MAKE) -C drti clean
	$(MAKE) -C passes clean
	$(MAKE) -C tests clean
