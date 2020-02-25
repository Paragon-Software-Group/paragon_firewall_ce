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

#include <optional>

namespace mcom {

template <class... Ts>
struct types {};

namespace types_details {

template <class Fn>
struct func_args_;

template <class R, class T, class... Args>
struct func_args_<R (T::*)(Args...)> {
  using type = types<Args...>;
};

template <class R, class T, class... Args>
struct func_args_<R (T::*)(Args...) const> {
  using type = types<Args...>;
};

template <class Fn>
struct func_args {
  using type =
      typename types_details::func_args_<decltype(&Fn::operator())>::type;
};

template <class R, class... Args>
struct func_args<R (*)(Args...)> {
  using type = types<Args...>;
};

template <class R, class... Args>
struct func_args<R (&)(Args...)> {
  using type = types<Args...>;
};

template <class L, class R>
struct concat_;

template <class... Ls, class... Rs>
struct concat_<types<Ls...>, types<Rs...>> {
  using type = types<Ls..., Rs...>;
};

template <class T>
struct drop_one_last_;

template <class T>
struct drop_one_last_<types<T>> {
  using type = types<>;
};

template <class T, class... Ts>
struct drop_one_last_<types<T, Ts...>> {
  using type =
      typename concat_<types<T>,
                       typename drop_one_last_<types<Ts...>>::type>::type;
};

template <size_t N, class T>
struct drop_last_;

template <class... Ts>
struct drop_last_<1, types<Ts...>> {
  using type = typename drop_one_last_<types<Ts...>>::type;
};

template <size_t N, class... Ts>
struct drop_last_<N, types<Ts...>> {
  using type =
      typename drop_last_<N - 1,
                          typename drop_one_last_<types<Ts...>>::type>::type;
};

template <bool ValidI, size_t I, class... Ts>
struct type_at_;

template <size_t I, class... Ts>
struct type_at_<true, I, Ts...> {
  using type = __type_pack_element<I, Ts...>;
};

template <size_t I, class... Ts>
struct type_at_<false, I, Ts...> {
  using type = void;
};

template <class T, class = void>
struct is_callable_ : std::false_type {};

template <class T>
struct is_callable_<T, std::void_t<decltype(&T::operator())>> : std::true_type {
};

}  // namespace types_details

template <class Fn>
using func_args_t = typename types_details::func_args<Fn>::type;

template <size_t I, class Types>
using drop_last_t = typename types_details::drop_last_<I, Types>::type;

template <int I, class... Ts>
using type_at_t = typename types_details::type_at_<
    (I >= 0 && I < sizeof...(Ts)) || (I < 0 && (-I) <= sizeof...(Ts)),
    I >= 0 ? I : sizeof...(Ts) + I, Ts...>::type;

template <class... Ts>
using last_t = type_at_t<-1, Ts...>;

template <class T>
inline constexpr bool is_callable_v =
    types_details::is_callable_<std::remove_reference_t<T>>::value;

template <class T>
struct is_tuple : std::false_type {};

template <class... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template <class T>
inline constexpr bool is_tuple_v = is_tuple<T>::value;

template <class T>
struct is_optional : std::false_type {};

template <class T>
struct is_optional<std::optional<T>> : std::true_type {};

template <class T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template <class T>
struct tuple_types;

template <class... Ts>
struct tuple_types<std::tuple<Ts...>> {
  using type = types<Ts...>;
};

template <class T>
using tuple_types_t = typename tuple_types<T>::type;

static_assert(
    std::is_same_v<types<int, char>, drop_last_t<1, types<int, char, double>>>);
static_assert(std::is_same_v<types<>, drop_last_t<1, types<int>>>);
static_assert(
    std::is_same_v<types<int>, drop_last_t<2, types<int, char, double>>>);
static_assert(
    std::is_same_v<types<>, drop_last_t<3, types<int, char, double>>>);

static_assert(std::is_same_v<double, last_t<int, char, double>>);
static_assert(std::is_same_v<void, last_t<>>);

static_assert(std::is_same_v<double, type_at_t<-1, int, char, double>>);
static_assert(std::is_same_v<int, type_at_t<-3, int, char, double>>);
static_assert(std::is_same_v<char, type_at_t<1, int, char, double>>);
static_assert(std::is_same_v<void, type_at_t<3, int, char, double>>);

template <class... Ts>
struct MakeTupleType_;

template <class... Ts>
struct MakeTupleType_<mcom::types<Ts...>> {
  using type = std::tuple<Ts...>;
};

template <class T>
using MakeTupleType = typename MakeTupleType_<T>::type;

template <class Out, class In, size_t... Is>
constexpr Out DropLast_(In &&in, std::index_sequence<Is...>) {
  return Out{std::get<Is>(std::forward<In>(in))...};
}

template <size_t N, class... Ts>
constexpr MakeTupleType<drop_last_t<N, types<Ts...>>> DropLast(
    std::tuple<Ts...> tpl) {
  using Result = MakeTupleType<drop_last_t<N, types<Ts...>>>;

  return DropLast_<Result>(std::forward<std::tuple<Ts...>>(tpl),
                           std::make_index_sequence<sizeof...(Ts) - N>{});
}

template <class... Ts>
constexpr last_t<Ts...> &&Last(Ts &&... args) {
  return std::get<sizeof...(Ts) - 1>(
      std::forward_as_tuple(std::forward<Ts>(args)...));
}

}  // namespace mcom
