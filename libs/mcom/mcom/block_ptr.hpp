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

#include <Block.h>

namespace mcom {

template <class Rt, class... Args>
class BlockPtr {
 public:
  typedef Rt (^Block)(Args...);

  BlockPtr() {}

  BlockPtr(Block ptr) : ptr_{ptr ? Block_copy(ptr) : Block(nullptr)} {}

  BlockPtr(const BlockPtr &other) : BlockPtr(other.ptr_) {}

  BlockPtr(BlockPtr &&);

  ~BlockPtr() {
    if (ptr_) {
      Block_release(ptr_);
    }
  }

  Rt operator()(Args... arg) const { return ptr_(std::forward<Args>(arg)...); }

  operator bool() const { return ptr_ != nullptr; }

  BlockPtr &operator=(const BlockPtr &other) {
    if (ptr_) {
      Block_release(ptr_);
    }
    ptr_ = other.ptr_ ? Block_copy(other.ptr_) : static_cast<Block>(nullptr);
    return *this;
  }

  BlockPtr &operator=(BlockPtr &&other) {
    if (ptr_) {
      Block_release(ptr_);
    }
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
    return *this;
  }

 private:
  Block ptr_;
};

}  // namespace mcom
