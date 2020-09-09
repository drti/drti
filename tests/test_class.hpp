// -*- mode:c++ -*-
//
// Header file test_class.hpp
//
// Class for demonstrating virtual function calls
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
// 2020/08/24   rmg     File creation
//

#ifndef test_class_rmg_20200824_included
#define test_class_rmg_20200824_included

#include <memory>

namespace drti_test
{
    struct interface
    {
        virtual ~interface() = default;
        virtual const void* virtual_function() const = 0;

        static std::unique_ptr<interface> create();
    };

    //! This is a workaround to allow us to name (via the
    //! drti_test_targets.txt file) the type of the virtual function
    //! calls that we want to inline.
    inline const void* type_matched_function(const interface*);
}

// We never actually call this but we need it to be available by name
// during the DRTI decoration pass
__attribute__((used)) inline const void* drti_test::type_matched_function(
    const interface*)
{
    return nullptr;
}

#endif // test_class_rmg_20200824_included
