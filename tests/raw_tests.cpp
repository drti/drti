// -*- mode:c++ -*-
//
// Module raw_tests.cpp
//
// Tests using the raw drti shared object library (see also
// intercept_tests.cpp)
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
// 2020/04/10   rmg     File creation
// 2020/08/17   rmg     Renamed from test_main.cpp to raw_tests.cpp
//

#include <iostream>
#include <cassert>

#include "test_support.hpp"

using test_function_type1 = const void* (*)();

enum class result_type { pass, fail, known_bug };

// Prevent inlining during ahead-of-time compilation
__attribute__((noinline)) static bool test1(const void*& last_result)
{
    // Inlining this kind of direct call is possible via link-time
    // optimisation as well, of course.
    const void* next_result = test_target1();

    if(!last_result)
    {
        last_result = next_result;
    }

    return next_result != last_result;
}

// Prevent inlining during ahead-of-time compilation. Note that we
// need a chain of at least two calls in order to inline anything at
// runtime
__attribute__((noinline)) static result_type test1()
{
    const void* last_result = nullptr;

    for(int count = 0; count < 1000; ++count)
    {
        if(test1(last_result))
        {
            assert(drti_test::get_counter("test_target1") == count + 1);
            // Success!
            std::cout << "test1 passed\n";
            return result_type::pass;
        }
    }
    std::cout << "test1 failed: return value never changed\n";
    return result_type::fail;
}

// Invocation via function pointer
__attribute__((noinline)) static bool invoke(
    test_function_type1 target, const void*& last_result)
{
    // Inlining this kind of function pointer call is difficult to
    // inline at link time
    const void* next_result = target();

    if(!last_result)
    {
        last_result = next_result;
    }

    return next_result != last_result;
}

__attribute__((noinline)) static result_type test2(int external_data)
{
    // This is just like test1 except it forces the choice of target
    // function to be dependent on data available only at runtime and
    // verifies inlining invocations done via a function pointer
    const test_function_type1 target = external_data > 1 ? test_target1 : test_target2;
    const char* counter_name = external_data > 1 ? "test_target1" : "test_target2";
    const void* last_result = nullptr;

    for(int count = 0; count < 1000; ++count)
    {
        if(invoke(target, last_result))
        {
            assert(drti_test::get_counter(counter_name) == count + 1);
            // Success!
            std::cout << "test2 passed\n";
            return result_type::pass;
        }
    }
    std::cout << "test2 failed: return value never changed\n";
    return result_type::fail;
}

__attribute__((noinline)) static result_type test3(int external_data)
{
    // This invokes a non-decorated function with a tail-call. Since
    // it isn't decorated its return value should never change
    const test_function_type1 target = test_target3;
    const char* counter_name = "test_target3";
    const void* last_result = nullptr;

    for(int count = 0; count < 1000; ++count)
    {
        if(invoke(target, last_result))
        {
            std::cout << "test3 known_bug: return value changed\n";
            return result_type::known_bug;
        }
        assert(drti_test::get_counter(counter_name) == count + 1);
    }
    std::cout << "test3 passed: return value never changed\n";
    return result_type::pass;
}

__attribute__((noinline)) static bool test4(const void*& last_result, bool do_throw)
{
    const void* next_result = test_target4(do_throw);

    if(!last_result)
    {
        last_result = next_result;
    }

    return next_result != last_result;
}

// Prevent inlining during ahead-of-time compilation. Note that we
// need a chain of at least two calls in order to inline anything at
// runtime
__attribute__((noinline)) static result_type test4()
{
    const void* last_result = nullptr;
    bool value_changed = false;

    for(int count = 0; count < 1000; ++count)
    {
        try
        {
            if(test4(last_result, value_changed))
            {
                value_changed = true;
            } 
            assert(drti_test::get_counter("test_target4") == count + 1);
        }
        catch (const std::runtime_error& error)
        {
            assert(error.what() == std::string("test_target4"));
            assert(drti_test::get_counter("test_target4") == count + 1);
            // Success!
            return result_type::pass;
        }
    }
    std::cout << "test4 failed: ";

    if(value_changed)
    {
        std::cout << "no exception thrown\n";
    }
    else
    {
        std::cout << "return value never changed\n";
    }

    return result_type::fail;
}

bool all_passed(int external_data)
{
    int tried = 0;
    int passed = 0;
    int known_bug = 0;

    auto check = [&](result_type result) {
        ++tried;
        switch(result)
        {
            case result_type::pass:
                ++passed;
                break;

            case result_type::known_bug:
                ++known_bug;
                break;

            case result_type::fail:
                break;
        }
    };

    check(test1());
    check(test2(external_data));
    check(test3(external_data));
    check(test4());

    std::cout
        << "Ran "
        << tried << " raw tests, "
        << passed << " passed, "
        << known_bug << " known bug(s), "
        << (tried - passed - known_bug) << " failed\n";

    return (passed + known_bug) == tried;
}

int main(int argc, char *argv[])
{
    return all_passed(argc) ? 0 : 1;
}
