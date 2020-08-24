// -*- mode:c++ -*-
//
// Header file configuration.hpp
//
// Copyright (c) 2019, 2020 Raoul M. Gough
//
// This file is part of DRTI.
//
// DRTI is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 only.
//
// DRTI is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// History
// =======
// 2019/10/28   rmg     File creation
//

#ifndef configuration_rmg_20191028_included
#define configuration_rmg_20191028_included

// TODO - make these C++ constants once asm.cpp no longer needs them
// as macros
#define DRTI_RETALIGN 32
#define DRTI_STASH_BYTES 8
#define DRTI_VERSION 1
#define DRTI_MAGIC (0xd511 + (DRTI_VERSION << 16))

namespace drti
{
  constexpr int housekeeping_interval = 1000;
}

#endif // configuration_rmg_20191028_included
