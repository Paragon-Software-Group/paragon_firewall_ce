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
//
// Created by Alexey Antipov on 2019-11-07.
//

#pragma once

#define MCOM_ON_ERROR(...)                        \
  mcom::Deferred on_error_##__COUNTER__ {         \
    [&]() {                                       \
      if (std::uncaught_exceptions()) __VA_ARGS__ \
    }                                             \
  }

namespace mcom {

class Deferred {
 public:
  using Handler = std::function<void()>;

  template <class Fn>
  explicit Deferred(Fn &&fn) : handler_{std::forward<Fn>(fn)} {}

  template <class Fn>
  static std::shared_ptr<Deferred> Shared(Fn &&fn) {
    return std::make_shared<Deferred>(std::forward<Fn>(fn));
  }

  Deferred(const Deferred &) = delete;

  Deferred &operator=(const Deferred &) = delete;

  Deferred(Deferred &&other) : handler_{std::move(other.handler_)} {
    other.handler_ = nullptr;
  }

  Deferred &operator=(Deferred &&);

  ~Deferred() {
    if (handler_) {
      handler_();
    }
  }

  void Cancel() { handler_ = nullptr; }

 private:
  Handler handler_;
};

}  // namespace mcom
