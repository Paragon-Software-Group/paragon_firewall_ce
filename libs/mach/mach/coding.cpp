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
// Created by Alexey Antipov on 18/02/2019.
//

#include "mach/coding.hpp"

#include <mach/mach_vm.h>

namespace mach {

Encoder &Encoder::AddBytes(const void *bytes, size_t size) {
  const auto old_size = buffer_.size();
  buffer_.resize(old_size + Align(size));
  std::memcpy(buffer_.data() + old_size, bytes, size);
  return *this;
}

mach_msg_ool_descriptor_t Encoder::CopyDescriptor() const {
  mach_vm_address_t address;
  mach_vm_allocate(mach_task_self(), &address, buffer_.size(),
                   VM_FLAGS_ANYWHERE);

  void *pointer = reinterpret_cast<void *>(address);

  std::memcpy(pointer, buffer_.data(), buffer_.size());

  mach_msg_ool_descriptor_t descriptor;
  descriptor.address = pointer;
  descriptor.copy = MACH_MSG_VIRTUAL_COPY;
  descriptor.deallocate = TRUE;
  descriptor.size = static_cast<mach_msg_size_t>(buffer_.size());
  descriptor.type = MACH_MSG_OOL_DESCRIPTOR;

  return descriptor;
}

std::string_view Decoder::DecodeString() {
  const size_t size = DecodeInt32();
  return {static_cast<const char *>(DecodeBytes(size)), size};
}

const void *Decoder::DecodeBytes(size_t size) {
  const void *base = address_;
  address_ = static_cast<const uint8_t *>(base) + Align(size);
  return base;
}

}  // namespace mach
