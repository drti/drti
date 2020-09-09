// -*- mode:c++ -*-
//
// Module test_class.cpp
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

#include "test_class.hpp"
#include "test_support.hpp"

namespace drti_test
{
    struct impl : interface
    {
        const void* virtual_function() const override;
    };
}

const void* drti_test::impl::virtual_function() const
{
    return instruction_pointer();
}

std::unique_ptr<drti_test::interface> drti_test::interface::create()
{
    return std::make_unique<impl>();
}

// Support interface->foo() virtual function calls to be inlined
// against impl->foo(). To avoid having to provide this information
// manually we'd need some DRTI-specific support in the C++ front-end
DRTI_CONVERTIBLE(drti_test::interface*, drti_test::impl*);
