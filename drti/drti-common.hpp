// -*- mode:c++ -*-
//
// Header file drti-common.hpp
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
// 2020/01/17   rmg     File creation
// 2020/04/19   rmg     File deleted (unused)
// 2020/08/03   rmg     File restored to centralise global variable filtering
//

#ifndef drti_common_rmg_20200117_included
#define drti_common_rmg_20200117_included

#include <functional>

namespace llvm
{
    class Module;
    class GlobalVariable;
}

namespace drti
{
    //! Visit the global variables from a module that need address
    //! equivalence between ahead-of-time compiled code and JIT code
    void visit_listed_globals(
        llvm::Module&,
        const std::function<void(llvm::GlobalVariable&)>&);
}

#endif // drti_common_rmg_20200117_included
