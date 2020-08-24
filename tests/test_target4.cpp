// -*- mode:c++ -*-
//
// Module test_target4.cpp
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

#include <stdexcept>

const void* test_target4(bool do_throw)
{
    static unsigned& counter = drti_test::new_counter("test_target4");
    ++counter;

    if(do_throw)
    {
        throw std::runtime_error("test_target4");
    }

    return drti_test::instruction_pointer();
}
