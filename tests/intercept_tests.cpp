// -*- mode:c++ -*-
//
// Module intercept_tests.cpp
//
// Tests that intercept calls that normally go to drtiruntime.so
//
// Copyright (c) 2020 Raoul M. Gough
//
// This file is part of DRTI.
//
// DRTI is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, version 3 only.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
// History
// =======
// 2020/08/17   rmg     File creation
//

#include <drti/tree.hpp>
#include <drti/runtime.hpp>

#include <vector>
#include <iostream>

#include "test_support.hpp"

static std::vector<drti::treenode*> s_inspected;

namespace drti
{
    void inspect_treenode(treenode*);
}

void drti::inspect_treenode(treenode* node)
{
    // Currently inspect_treenode is called at most once per node, so
    // we simply push the node onto a vector for later inspection.
    s_inspected.push_back(node);
}

//! Call a leaf function for the call tree
__attribute__((noinline)) void call_leaf()
{
    test_target1();
}

__attribute__((noinline)) void test1()
{
    // Call a leaf function repeatedly and check that inspect_treenode
    // got called at some point
    for(int count = 0; count < 1000; count += 1)
    {
        call_leaf();
    }
    assert(!s_inspected.empty());
    assert(s_inspected.size() == 1);
    assert(s_inspected.front()->parent == nullptr);
    // Check the caller and callee names
    assert(std::string("_Z9call_leafv") == s_inspected.front()->location.landing.function_name);
    assert(std::string("_Z12test_target1v") == s_inspected.front()->landing->function_name);
}

int main(int argc, char *argv[])
{
    test1();

    std::cout << "intercept_tests passed\n";

    return 0;
}
