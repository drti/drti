// -*- mode:c++ -*-
//
// Module test_target3.cpp
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
// 2020/08/03   rmg     File creation
//

#include "test_support.hpp"

// This test function is *not* listed in test_main_drti_targets.txt so
// it does not get DRTI-decorated. However it makes a tail call to
// test_target2 which does get decorated and we may wrongly conclude
// that our caller calls test_target2 directly.
//
// TODO - handle this case correctly.
const void* test_target3()
{
    static unsigned& counter = drti_test::new_counter("test_target3");
    ++counter;

    // Can be tail-call optimised
    return test_target2();
}
