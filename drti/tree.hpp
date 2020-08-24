// -*- mode:c++ -*-
//
// Header file tree.hpp
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
// 2019/10/28   rmg     File creation
//

#ifndef tree_rmg_20191028_included
#define tree_rmg_20191028_included

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdatomic.h>  // C11 atomics simplify the bitcode DRTI generates
#include <vector>

#include <drti/configuration.hpp>
#include <drti/runtime.hpp>

namespace drti
{
  using counter_t = _Atomic(int64_t);

  constexpr int abi_version = DRTI_VERSION;

  //! Runtime access to the bitcode
  struct reflect
  {
      //! Pointer to the bitcode for the containing module
      const char* module = 0;
      //! Size of the bitcode
      size_t module_size = 0;
      //! Pointer to the array of addresses of globals referenced by
      //! the bitcode
      void* const* globals = 0;
      //! Number of globals in the array
      size_t globals_size = 0;
  };

  //! Function entry point accounting
  struct landing_site
  {
    //! Total number of times this entry point was hit
    counter_t total_called = 0;
    //! Name of the global variable referencing this landing_site
    const char* global_name = 0;
    //! Name of the unique function that references the global
    const char* function_name = 0;
    //! Link to the bitcode for the containing module
    reflect* self = nullptr;
  };

  struct treenode;

  //! Static information about a call site, i.e. unique to the calling
  //! location
  //! TODO - for initialisation order safety we need this to be statically initialisable
  struct static_callsite
  {
      //! Total calls eminating from this site, regardless of caller and
      //! callee
      counter_t total_calls = 0;
      //! The entry point of the function containing this call site
      landing_site& landing;
      //! The number of the call instruction within the calling
      //! function, counting from zero. We assume that iterating the
      //! function IR at run-time gives the same sequence as during
      //! ahead-of-time compilation when this number was recorded.
      unsigned call_number;
      //! Node for each call chain passing through this call site.
      std::vector<std::unique_ptr<treenode>> nodes;
  };

  //! A node in a call tree, representing one (parent, target) pair
  //! from one static callsite
  struct treenode
  {
    //! For runtime detection of abi mismatch between caller and
    //! landing
    const int caller_abi_version = abi_version;
    //! Call count for this (parent, target) pair
    counter_t chain_calls = 0;
    //! The static location of the callsite for this node
    static_callsite& location;
    //! Upwards in the chain
    treenode* const parent;
    //! The function address the caller used
    const void* const target;
    //! Either the original target or a JIT-compiled version of the
    //! function addressed by the original target
    const void* resolvedTarget;
    //! In the absence of what I'm going to call "evil thunking" there
    //! is exactly one landing_site per target function addresss. In
    //! theory it would be possible for one target address to arrive
    //! at different landing sites, if the call goes via a thunk that
    //! can change destination. Does that actually exist in practice?
    landing_site* landing;
  };
}

#endif // tree_rmg_20191028_included
