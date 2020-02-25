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

#include <system_error>

#include "utility.hpp"

namespace mcom {

template <class T>
class Result {
 public:
  constexpr Result() noexcept : got_value_(true), value_() {}

  Result(Result &&e) : got_value_(e.got_value_) {
    if (got_value_) {
      new (std::addressof(value_)) T(std::move(e.value_));
    } else {
      using Ec = std::error_code;
      new (std::addressof(code_)) Ec(e.code_);
    }
  }

  template <class... Us>
  explicit Result(in_place_t, Us &&... args)
      : got_value_(true), value_(std::forward<Us>(args)...) {}

  Result(const std::error_code &ec) : got_value_(false), code_(ec) {}

  template <class U = T,
            std::enable_if_t<
                conjunction<std::is_constructible<T, U &&>,
                            negation<std::is_same<std::decay_t<U>, in_place_t>>,
                            negation<std::is_same<std::decay_t<U>, Result<T>>>,
                            std::is_convertible<U &&, T>>::value,
                bool> = false>
  Result(T &&value) : got_value_(true), value_(std::forward<T>(value)) {}

  template <class U = T,
            std::enable_if_t<
                conjunction<std::is_constructible<T, U &&>,
                            negation<std::is_same<std::decay_t<U>, in_place_t>>,
                            negation<std::is_same<std::decay_t<U>, Result<T>>>,
                            negation<std::is_convertible<U &&, T>>>::value,
                bool> = false>
  explicit Result(T &&value)
      : got_value_(true), value_(std::forward<T>(value)) {}

  const T &Value() const &;

  T &Value() & {
    if (got_value_) {
      return value_;
    } else {
      throw std::system_error{code_};
    }
  }

  const T &&Value() const &&;

  T &&Value() && {
    if (got_value_) {
      return std::move(value_);
    } else {
      throw std::system_error{code_};
    }
  }

  const T *operator->() const { return std::addressof(value_); }
  T *operator->() { return std::addressof(value_); }

  const T &operator*() const & { return value_; }
  T &operator*() & { return value_; }

  const T &&operator*() const &&;
  T &&operator*() && { return std::move(value_); }

  const std::error_code &Code() const { return code_; }

  void Raise() const;

  explicit operator bool() const noexcept { return got_value_; }

  bool HasValue() const noexcept { return got_value_; }

  template <class U = T>
  T ValueOr(U &&default_value) && {
    if (*this) {
      return std::move(*this).Value();
    } else {
      return std::forward<U>(default_value);
    }
  }

  ~Result() {
    if (got_value_) {
      value_.~T();
    } else {
      using Ec = std::error_code;
      code_.~Ec();
    }
  }

 private:
  bool got_value_;
  union {
    T value_;
    std::error_code code_;
  };
};

template <class T>
class Result<T &> {
 public:
  Result(T &value) : got_value_{true}, value_{value} {}

  Result(Result &&e) : got_value_{e.got_value_} {
    if (got_value_) {
      new (std::addressof(value_))
          std::reference_wrapper<T>(std::move(e.value_));
    } else {
      using Ec = std::error_code;
      new (std::addressof(code_)) Ec(e.code_);
    }
  }

  explicit Result(in_place_t, T &arg) : got_value_{true}, value_{arg} {}

  Result(const std::error_code &ec) : got_value_(false), code_(ec) {}

  const T &Value() const &;

  T &Value() & {
    if (got_value_) {
      return value_;
    } else {
      throw std::system_error{code_};
    }
  }

  const T &&Value() const &&;

  T &&Value() && {
    if (got_value_) {
      return std::move(value_);
    } else {
      throw std::system_error{code_};
    }
  }

  const T *operator->() const { return std::addressof(value_); }
  T *operator->() { return std::addressof(value_); }

  const T &operator*() const & { return value_; }
  T &operator*() & { return value_; }

  const T &&operator*() const &&;
  T &&operator*() && { return std::move(value_); }

  const std::error_code &Code() const { return code_; }

  void Raise() const;

  explicit operator bool() const noexcept { return got_value_; }

  bool HasValue() const noexcept { return got_value_; }

  ~Result() {
    if (!got_value_) {
      using Ec = std::error_code;
      code_.~Ec();
    }
  }

 private:
  bool got_value_;
  union {
    std::reference_wrapper<T> value_;
    std::error_code code_;
  };
};

template <class T, class U>
bool operator==(const Result<T> &e, const U &v) {
  return bool(e) && *e == v;
}

template <class T, class U>
bool operator>(const Result<T> &e, const U &v) {
  return bool(e) && (*e > v);
}

void Check(const std::error_code &ec);

/// Run void function-object f, return error code.
template <class F>
auto Expect(F f) -> std::enable_if_t<std::is_same<void, decltype(f())>::value,
                                     std::error_code> {
  try {
    f();
    return {};
  } catch (const std::system_error &e) {
    return e.code();
  }
}

/// Run function-object f returning T, return Result<T> if exception is
/// thrown.
template <class F>
auto Expect(F f) -> std::enable_if_t<!std::is_same<void, decltype(f())>::value,
                                     Result<decltype(f())>> {
  try {
    return f();
  } catch (const std::system_error &e) {
    return e.code();
  }
}

}  // namespace mcom

#define MCOM_CHECK(code) mcom::Check(code)
