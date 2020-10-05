// -*- mode:c++ -*-
//
// Module drti-inline.cpp
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
// This compilation unit provides raw bitcode for use by the drti
// pass(es)
//
// History
// =======
// 2020/01/06   rmg     File creation
//

// Get type definitions
#include <drti/runtime.hpp>

// We put the inlinable functions in the global namespace with C
// linkage just to avoid the complication of using C++ name mangling
// to access them from drti-decorate
#define DRTI_INLINE_SUPPORT extern "C" inline __attribute__((always_inline, used))
#define DRTI_INTRINSIC extern "C"
using namespace drti;

#define DRTI_LIKELY( COND ) \
  __builtin_expect( static_cast<bool>(COND), 1 )

#define DRTI_UNLIKELY( COND ) \
  __builtin_expect( static_cast<bool>(COND), 0 )

#define DRTI_ATOMIC_INC( COUNT ) atomic_fetch_add(&COUNT, 1)

#define DRTI_CALL( CALL_SITE, CALLER, FPOINTER )                        \
    drti::treenode* CALL_SITE ## _drti_node =                           \
        _drti_call_from(                                                \
            CALL_SITE, CALLER, reinterpret_cast<void*>(FPOINTER));      \
    (CALL_SITE ## _drti_node ?                                          \
     reinterpret_cast<decltype(FPOINTER)>(                              \
         const_cast<void*>(CALL_SITE ## _drti_node->resolved_target)) : \
     (FPOINTER))

// These functions are declared but don't exist and get rewritten by
// our MachineFunctionPass, acting as a sort of "poor person's"
// intrinsics
DRTI_INTRINSIC drti::treenode* _drti_caller();
DRTI_INTRINSIC void _drti_set_caller(drti::treenode*);

DRTI_INLINE_SUPPORT treenode* _drti_lookup_or_insert(
    static_callsite& site,
    treenode* caller,
    const void* target)
{
    for(const std::unique_ptr<treenode>& node: site.nodes)
    {
        if(node->parent == caller && node->target == target)
        {
            return node.get();
        }
    }

    if(caller)
    {
        assert(caller->caller_abi_version == abi_version);
    }

    // resolved_target can be modified later and we initialize it here
    // to the same target
    std::unique_ptr<treenode> new_node(
        new treenode{abi_version, 0, site, caller, target, target, nullptr});
                                     
    site.nodes.emplace_back(std::move(new_node));

    return site.nodes.back().get();
}

DRTI_INLINE_SUPPORT treenode* _drti_call_from(
    static_callsite& site, treenode* caller, const void* target)
{
    DRTI_ATOMIC_INC(site.total_calls);
    // Here we allow null callers for the creation of tree roots
    treenode& node(*_drti_lookup_or_insert(site, caller, target));
    DRTI_ATOMIC_INC(node.chain_calls);
    return &node;
}

DRTI_INLINE_SUPPORT void _drti_landed(landing_site& site, treenode* caller)
{
    DRTI_ATOMIC_INC(site.total_called);

    // We don't do anything special here when site.total_called crosses
    // the house-keeping threshold, to avoid extra costs when there is
    // no caller information.
    if(DRTI_UNLIKELY(caller))
    {
        if(DRTI_LIKELY(caller->landing))
        {
            assert(caller->landing == &site);
        }
        else
        {
            assert(caller->caller_abi_version == abi_version);
            // TODO - detect landing after jumps from tail-optimized calls
            caller->landing = &site;
            inspect_treenode(caller);
        }
    }
}
