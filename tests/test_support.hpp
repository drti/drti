// -*- mode:c++ -*-
//
// Header file test_support.hpp
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

#ifndef test_support_rmg_20200803_included
#define test_support_rmg_20200803_included

//! Functions that give us back the address of an arbitrary
//! instruction in their own machine code.  We use these to confirm
//! runtime recompilation.
extern const void* test_target1();
extern const void* test_target2();
extern const void* test_target3();
extern const void* test_target4(bool);

namespace drti_test
{
    //! Return the current value of the instruction pointer register
    inline const void* instruction_pointer();

    //! Allocate a new counter with given name. This will abort if
    //! invoked more than once for the same name, to help detect
    //! re-initialisation of static data.
    unsigned& new_counter(const char* name);

    //! Get a counter with the given name. Aborts if the name doesn't
    //! exist to help detect non-invocation of static data
    //! initialisers.
    const unsigned& get_counter(const char* name);
}

__attribute__((always_inline)) inline const void* drti_test::instruction_pointer()
{
    const void* result = nullptr;

    // Return the address of our code to reveal recompilation at
    // runtime.  In theory we could use the gcc and clang extension
    // which allows taking the addres of a label, e.g. label: result =
    // &&label; but in fact that defeats inlining at runtime so we use
    // inline assembly to read the instruction pointer
    asm volatile(
        "    LEAQ (%%RIP), %0\n\t"
        : "=r" (result)
        :
        :
        );

    return result;
}

#endif // test_support_rmg_20200803_included
