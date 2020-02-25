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
#include <type_traits>

namespace mcom {

template <class...>
struct void_s {
  typedef void type;
};

template <class... Ts>
using void_t = typename void_s<Ts...>::type;

struct in_place_t {
  explicit in_place_t() = default;
};

static constexpr in_place_t in_place{};

template <class...>
struct conjunction : std::true_type {};
template <class B>
struct conjunction<B> : B {};
template <class B, class... Bs>
struct conjunction<B, Bs...>
    : std::conditional_t<bool(B::value), conjunction<Bs...>, B> {};

template <class...>
struct disjunction : std::false_type {};
template <class B>
struct disjunction<B> : B {};
template <class B, class... Bs>
struct disjunction<B, Bs...>
    : std::conditional_t<bool(B::value), B, disjunction<Bs...>> {};

template <class B>
struct negation : std::integral_constant<bool, !bool(B::value)> {};

}  // namespace mcom

static inline void mcom_defer_call(std::function<void()> *f) { (*f)(); }

#define MCOM_DEFER_IMPL(var)                                               \
  std::function<void()> var __attribute__((__cleanup__(mcom_defer_call))); \
  var = [&]()

#define MCOM_DEFER() MCOM_DEFER_IMPL(__mcom_defer_##__COUNTER__)

#define MCOM_BITMASK(type)                                               \
  inline type operator|(type a, type b) {                                \
    return static_cast<type>(static_cast<int>(a) | static_cast<int>(b)); \
  }                                                                      \
                                                                         \
  inline bool operator&(type a, type b) {                                \
    return 0 != (static_cast<int>(a) & static_cast<int>(b));             \
  }
