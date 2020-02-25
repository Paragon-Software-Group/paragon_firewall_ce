// Paragon Firewall Community Edition
// Copyright (C) 2019-2020  Paragon Software GmbH
//
// This file is part of Paragon Firewall Community Edition.
//
// Paragon Firewall Community Edition is free software: you can
// redistribute it and/or modify it under the terms of the GNU General
// Public License as published by the Free Software Foundation, either
// version 3 of the License, or (at your option) any later version.
//
// Paragon Firewall Community Edition is distributed in the hope that it
// will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
// the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Paragon Firewall Community Edition. If not, see
//   <https://www.gnu.org/licenses/>.

#pragma once

#include <functional>
#include <tuple>

#include <mcom/dispatch.hpp>
#include <mcom/types.hpp>

namespace mach {

namespace server_internal {

template <class Fn, class... Args>
struct AsyncHandler;

template <class Fn, class... Args>
struct AsyncHandler<Fn, mcom::types<Args...>> {
  Fn handler;
  dispatch::Group group;
  dispatch::Queue queue;

  void operator()(Args... args) const {
    std::tuple<Args...> args_tuple{std::move(args)...};
    group.Async(queue, [this, args = std::move(args_tuple)]() mutable {
      std::apply(handler, std::move(args));
    });
  }
};

template <class Fn>
using AsyncHandlerFor = AsyncHandler<Fn, mcom::func_args_t<Fn>>;

}  // namespace server_internal

}  // namespace mach
