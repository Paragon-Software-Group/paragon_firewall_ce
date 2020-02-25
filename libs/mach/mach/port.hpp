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
// Created by Alexey Antipov on 13/02/2019.
//

#pragma once

#include <utility>

#include <mach/mach.h>

namespace mach {

enum class PortRightType : mach_port_right_t {
  Receive = MACH_PORT_RIGHT_RECEIVE,
  Send = MACH_PORT_RIGHT_SEND,
  SendOnce = MACH_PORT_RIGHT_SEND_ONCE
};

template <PortRightType Type>
class PortRightBase {
 public:
  mach_port_name_t Name() const noexcept { return name_; }

  PortRightBase(PortRightBase &&other) : name_{other.name_} {
    other.name_ = MACH_PORT_NULL;
  }

  PortRightBase &operator=(PortRightBase &&other) {
    Invalidate();
    std::swap(name_, other.name_);
    return *this;
  }

  ~PortRightBase() { Invalidate(); }

  void Invalidate() {
    if (name_ == MACH_PORT_NULL) {
      return;
    }

    mach_port_right_t right;

    switch (Type) {
      case PortRightType::Receive:
        right = MACH_PORT_RIGHT_RECEIVE;
        break;

      case PortRightType::Send:
        right = MACH_PORT_RIGHT_SEND;
        break;

      case PortRightType::SendOnce:
        right = MACH_PORT_RIGHT_SEND_ONCE;
        break;
    }

    mach_port_mod_refs(mach_task_self(), name_, right, -1);

    name_ = MACH_PORT_NULL;
  }

  mach_port_name_t Extract() && {
    const auto name = name_;
    name_ = 0;
    return name;
  }

 private:
  explicit PortRightBase(mach_port_name_t name) noexcept : name_{name} {}

  mach_port_name_t name_;
};

template <PortRightType Type>
class PortRight : public PortRightBase<Type> {
 public:
  using PortRightBase<Type>::PortRightBase;

  static PortRight Construct(mach_port_name_t name) noexcept {
    return PortRight{name};
  }
};

template <>
class PortRight<PortRightType::Receive>
    : public PortRightBase<PortRightType::Receive> {
 public:
  using PortRightBase<PortRightType::Receive>::PortRightBase;

  static PortRight Allocate() noexcept {
    mach_port_name_t name;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &name);
    return Construct(name);
  }

  static PortRight Construct(mach_port_name_t name) noexcept {
    return PortRight{name};
  }
};

using ReceiveRight = PortRight<PortRightType::Receive>;
using SendRight = PortRight<PortRightType::Send>;
using SendOnceRight = PortRight<PortRightType::SendOnce>;

}  // namespace mach
