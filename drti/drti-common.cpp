// -*- mode:c++ -*-
//
// Module drti-common.cpp
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

#include "drti-common.hpp"

#include "llvm/IR/Module.h"
#include "llvm/IR/GlobalVariable.h"

void drti::visit_listed_globals(
    llvm::Module& module,
    const std::function<void(llvm::GlobalVariable&)>& callback)
{
    for(llvm::GlobalVariable& variable: module.globals())
    {
        // TODO - handle (non-constant) variable definitions as well.
        // These also have to resolve against the globals from the
        // ahead-of-time compilation. Probably constants are different
        // because we want their values at (JIT) compile-time, but
        // then again if they're large or their addresses escape we
        // could be in trouble.
        // if(variable.isDeclaration())
        if(variable.isConstant())
        {
            // TODO - don't force address equality for constants
            // because we want their values at (JIT) compile-time.
            // However if their addresses are taken this could break
            // code.
        }
        else if(variable.getName().startswith("llvm."))
        {
            // We don't want to interfere with magic "variables" like
            // llvm.global_ctors or llvm.used
        }
        else
        {
            callback(variable);
        }
    }
}
