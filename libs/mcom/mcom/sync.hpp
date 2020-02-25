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

#include <mcom/dispatch.hpp>

namespace mcom {

template <class T>
class Sync {
 public:
  template <class... Args>
  Sync(Args &&... args) : value_{std::forward<Args>(args)...} {}

  Sync &operator=(Sync &&) = delete;

  class Guard {
   public:
    T *operator->() { return std::addressof(sync_.value_); }

    T &operator*() { return sync_.value_; }

    ~Guard() { sync_.sema_.Signal(); }

    Guard &operator=(Guard &&) = delete;

   private:
    friend class Sync;

    Guard(Sync &sync) : sync_{sync} {
      sync_.sema_.Wait(dispatch::Time::kForever);
    }

    Sync &sync_;
  };

  class ConstGuard {
   public:
    const T *operator->() { return std::addressof(sync_.value_); }

    const T &operator*() { return sync_.value_; }

    ~ConstGuard() { sync_.sema_.Signal(); }

    ConstGuard &operator=(Guard &&) = delete;

   private:
    friend class Sync;

    ConstGuard(const Sync &sync) : sync_{sync} {
      sync_.sema_.Wait(dispatch::Time::kForever);
    }

    const Sync &sync_;
  };

  Guard Locked() { return Guard{*this}; }

  ConstGuard Locked() const { return ConstGuard{*this}; }

  template <class Fn>
  auto Use(const Fn &fn) -> decltype(fn(std::declval<T &>())) {
    Guard guard{*this};
    return fn(value_);
  }

  template <class Fn>
  auto Use(Fn &&fn) -> decltype(fn(std::declval<T &>())) {
    Guard guard{*this};
    return fn(value_);
  }

  template <class Fn>
  auto Use(const Fn &fn) const -> decltype(fn(std::declval<T &>())) {
    ConstGuard guard{*this};
    return fn(value_);
  }

  T &AccessUnsafely() { return value_; }

  const T &AccessUnsafely() const { return value_; }

 private:
  friend class Guard;

  mutable dispatch::Semaphore sema_{1};
  T value_;
};

}  // namespace mcom
