// -*- mode:c++ -*-
//
// Module test_support.cpp
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
// 2020/08/03   rmg     File creation from counters.cpp
//

#include "test_support.hpp"

#include <map>
#include <string>
#include <cassert>

static std::map<std::string, unsigned>& global_map()
{
    static std::map<std::string, unsigned> counters;
    return counters;
}

unsigned& drti_test::new_counter(const char* name)
{
    std::map<std::string, unsigned>& counters(global_map());

    std::string nameString(name);

    assert(counters.find(nameString) == counters.end());
    return counters[nameString];
}

const unsigned& drti_test::get_counter(const char* name)
{
    std::map<std::string, unsigned>& counters(global_map());

    std::string nameString(name);

    assert(counters.find(nameString) != counters.end());
    return counters[nameString];
}

