// -*- mode:c++ -*-
//
// Header file runtime.hpp
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
// 2019/11/25   rmg     File creation
//

#ifndef runtime_rmg_20191125_included
#define runtime_rmg_20191125_included

#define DRTI_PUBLIC __attribute__((visibility("default")))

namespace drti
{
    struct treenode;

    //! Called by the client for treenodes that may be of interest
    DRTI_PUBLIC void inspect_treenode(treenode*);
}

#endif // runtime_rmg_20191125_included
