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

#include <new>
#include <type_traits>

#include "mcom/utility.hpp"

namespace mcom {

struct nullopt_t {
  explicit constexpr nullopt_t(int) {}
};

static constexpr nullopt_t nullopt{0};

template <class T>
class Optional {
 public:
  constexpr Optional() noexcept : got_value_(false) {}

  constexpr Optional(nullopt_t) noexcept : got_value_(false) {}

  constexpr Optional(const Optional &o) : got_value_(o.got_value_) {
    if (got_value_) {
      new (std::addressof(value_)) T(*o);
    }
  }

  template <
      std::enable_if_t<std::is_move_constructible<T>::value, bool> = false>
  Optional(Optional &&o) : got_value_(o.got_value_) {
    if (got_value_) {
      new (std::addressof(value_)) T(std::move(*o));
    }
  }

  template <
      class... Us,
      std::enable_if_t<std::is_constructible<T, Us...>::value, bool> = false>
  constexpr explicit Optional(in_place_t, Us &&... args)
      : got_value_(true), value_(std::forward<Us>(args)...) {}

  template <
      class U = T,
      std::enable_if_t<
          conjunction<std::is_constructible<T, U &&>,
                      negation<std::is_same<std::decay_t<U>, in_place_t>>,
                      negation<std::is_same<std::decay_t<U>, Optional<T>>>,
                      std::is_convertible<U &&, T>>::value,
          bool> = false>
  Optional(U &&value) : got_value_(true), value_(std::forward<U>(value)) {}

  template <
      class U = T,
      std::enable_if_t<
          conjunction<std::is_constructible<T, U &&>,
                      negation<std::is_same<std::decay_t<U>, in_place_t>>,
                      negation<std::is_same<std::decay_t<U>, Optional<T>>>,
                      negation<std::is_convertible<U &&, T>>>::value,
          bool> = false>
  explicit Optional(U &&value);

  Optional &operator=(nullopt_t) noexcept;

  Optional &operator=(const Optional &other) noexcept(
      std::is_nothrow_copy_constructible<T>::value
          &&std::is_nothrow_copy_assignable<T>::value) {
    if (other.got_value_) {
      if (got_value_) {
        value_ = other.value_;
      } else {
        got_value_ = true;
        new (std::addressof(value_)) T(other.value_);
      }
    } else {
      if (got_value_) {
        got_value_ = false;
        value_.~T();
      }
    }
    return *this;
  }

#if 1
  Optional &operator=(Optional &&other) noexcept(
      std::is_nothrow_move_assignable<T>::value
          &&std::is_nothrow_move_constructible<T>::value) {
    if (other.got_value_) {
      if (got_value_) {
        value_ = std::move(other.value_);
      } else {
        got_value_ = true;
        new (std::addressof(value_)) T{std::move(other.value_)};
      }
    } else {
      if (got_value_) {
        got_value_ = false;
        value_.~T();
      }
    }
    return *this;
  }

  template <
      class U = T,
      std::enable_if_t<conjunction<
          negation<std::is_same<std::decay_t<U>, Optional<T>>>,
          std::is_constructible<T, U>, std::is_assignable<T &, U>,
          disjunction<std::is_scalar<T>,
                      negation<std::is_same<std::decay_t<U>, T>>>>::value>>
  Optional &operator=(U &&);

  template <class U, std::enable_if_t<negation<conjunction<
                         std::is_constructible<T, Optional<U> &>,
                         std::is_constructible<T, const Optional<U> &>,
                         std::is_constructible<T, Optional<U> &&>,
                         std::is_constructible<T, const Optional<U> &&>,
                         std::is_convertible<Optional<U> &, T>,
                         std::is_convertible<const Optional<U> &, T>,
                         std::is_convertible<Optional<U> &&, T>,
                         std::is_convertible<const Optional<U> &&, T>,
                         std::is_assignable<T &, Optional<U> &>,
                         std::is_assignable<T &, const Optional<U> &>,
                         std::is_assignable<T &, Optional<U> &&>,
                         std::is_assignable<T &, const Optional<U> &&>,
                         std::is_constructible<T, const U &>,
                         std::is_assignable<T &, const U &>>>::value>>
  Optional &operator=(const Optional<U> &);

  template <
      class U,
      std::enable_if_t<negation<conjunction<
          std::is_constructible<T, Optional<U> &>,
          std::is_constructible<T, const Optional<U> &>,
          std::is_constructible<T, Optional<U> &&>,
          std::is_constructible<T, const Optional<U> &&>,
          std::is_convertible<Optional<U> &, T>,
          std::is_convertible<const Optional<U> &, T>,
          std::is_convertible<Optional<U> &&, T>,
          std::is_convertible<const Optional<U> &&, T>,
          std::is_assignable<T &, Optional<U> &>,
          std::is_assignable<T &, const Optional<U> &>,
          std::is_assignable<T &, Optional<U> &&>,
          std::is_assignable<T &, const Optional<U> &&>,
          std::is_constructible<T, U>, std::is_assignable<T &, U>>>::value>>
  Optional &operator=(Optional<U> &&);

#else
  Optional &operator=(T &&value) {
    if (got_value_) {
      value_ = std::move(value);
    } else {
      new (std::addressof(value_)) T(std::move(value));
      got_value_ = true;
    }
    return *this;
  }

  Optional &operator=(Optional &&o) {
    if (got_value_) {
      if (o.got_value_) {
        value_ = std::move(*o);
        o.reset();
      } else {
        reset();
      }
    } else if (o.got_value_) {
      got_value_ = true;
      new (std::addressof(value_)) T(std::move(*o));
      o.reset();
    }
    return *this;
  }
#endif

  const T *operator->() const { return std::addressof(value_); }
  T *operator->() { return std::addressof(value_); }

  const T &operator*() const & { return value_; }
  T &operator*() & { return value_; }

  const T &&operator*() const && { return value_; }
  T &&operator*() && { return std::move(value_); }

  T &Value() & { return value_; }
  const T &Value() const &;

  T &&Value() && { return std::move(value_); }
  const T &&Value() const &&;

  template <class U>
  auto ValueOr(U &&default_value)
      const & -> std::enable_if_t<std::is_copy_constructible<T>::value &&
                                      std::is_convertible<U &&, T>::value,
                                  T> {
    return bool(*this) ? **this
                       : static_cast<T>(std::forward<U>(default_value));
  }

  template <class U>
  auto ValueOr(U &&default_value)
      && -> std::enable_if_t<std::is_move_constructible<T>::value &&
                                 std::is_convertible<U &&, T>::value,
                             T> {
    return bool(*this) ? std::move(**this)
                       : static_cast<T>(std::forward<U>(default_value));
  }

  explicit operator bool() const noexcept { return got_value_; }

  bool HasValue() const noexcept { return got_value_; }

  void Reset() {
    if (got_value_) {
      value_.~T();
      got_value_ = false;
    }
  }

  ~Optional() {
    if (got_value_) {
      value_.~T();
    }
  }

 private:
  bool got_value_;
  union {
    T value_;
  };
};

template <class T>
bool operator!=(const Optional<T> &lhs, const T &rhs) {
  return lhs && (*lhs == rhs);
}

template <class T>
bool operator!=(const Optional<T> &lhs, const Optional<T> &rhs) {
  if (lhs) {
    return (!rhs) || (*lhs != *rhs);
  } else {
    return bool(rhs);
  }
}

template <class T>
inline bool operator==(const Optional<T> &lhs, const T &rhs) {
  return lhs && (*lhs == rhs);
}

template <class T>
struct opt_type {
  using type = typename mcom::Optional<T>;
};

template <class T>
struct opt_type<mcom::Optional<T>> {
  using type = typename opt_type<T>::type;
};

template <class T>
using opt_type_t = typename opt_type<T>::type;

static_assert(std::is_same<Optional<int>, opt_type_t<int>>::value, "");
static_assert(std::is_same<Optional<int>, opt_type_t<Optional<int>>>::value,
              "");
static_assert(
    std::is_same<Optional<int>, opt_type_t<Optional<Optional<int>>>>::value,
    "");

}  // namespace mcom
