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
// Created by Alexey Antipov on 05/03/2019.
//

#include "uuid.hpp"

#include <libkern/OSByteOrder.h>

namespace {

GUID SwapGUID(const GUID &in_guid) {
  GUID out_guid;

  out_guid.Data1 = OSSwapInt32(in_guid.Data1);
  out_guid.Data2 = OSSwapInt16(in_guid.Data2);
  out_guid.Data3 = OSSwapInt16(in_guid.Data3);
  std::memcpy(&out_guid.Data4, &in_guid.Data4, sizeof(GUID::Data4));

  return out_guid;
}

}  // namespace

namespace mcom {

Uuid::Uuid(const uuid_t &uuid) { uuid_copy(value_, uuid); }

Uuid::Uuid(const GUID &in_guid) {
  static_assert(sizeof(GUID) == sizeof(uuid_t), "");

  auto &out_guid = *reinterpret_cast<GUID *>(&value_);
  out_guid = SwapGUID(in_guid);
}

std::string Uuid::ToString() const {
  uuid_string_t uuid_str;
  uuid_unparse_lower(value_, uuid_str);
  return uuid_str;
}

GUID Uuid::ToGUID() const {
  auto &guid = *reinterpret_cast<const GUID *>(&value_);
  return SwapGUID(guid);
}

Optional<Uuid> Uuid::FromString(std::string_view uuid_str) {
  if (uuid_str.size() != 36) {
    return nullopt;
  }

  uuid_string_t uuid_str_buffer;
  std::memcpy(uuid_str_buffer, uuid_str.data(), 36);
  uuid_str_buffer[36] = '\0';

  uuid_t uuid;
  if (0 != uuid_parse(uuid_str_buffer, uuid)) {
    return nullopt;
  }

  return uuid;
}

}  // namespace mcom

namespace std {

size_t hash<mcom::Uuid>::operator()(const mcom::Uuid &uuid) const {
  return hash<std::string>{}(uuid.ToString());
}

}  // namespace std
