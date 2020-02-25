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

#pragma once

#include <string>

#include <uuid/uuid.h>

#include <mcom/optional.hpp>

// if windows GUID is not here, use this
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
  unsigned int Data1;
  unsigned short Data2;
  unsigned short Data3;
  unsigned char Data4[8];
} GUID;  // sizeof() == 16
#endif

namespace mcom {

class Uuid {
 public:
  Uuid() : value_{} {}

  Uuid(const uuid_t &uuid);

  Uuid(const GUID &guid);

  const uuid_t &Value() const { return value_; }

  std::string ToString() const;

  GUID ToGUID() const;

  static Optional<Uuid> FromString(std::string_view uuid_str);

 private:
  uuid_t value_;
};

inline bool operator==(const Uuid &lhs, const Uuid &rhs) {
  return 0 == uuid_compare(lhs.Value(), rhs.Value());
}

inline bool operator!=(const Uuid &lhs, const Uuid &rhs) {
  return 0 != uuid_compare(lhs.Value(), rhs.Value());
}

}  // namespace mcom

namespace std {

template <>
struct hash<mcom::Uuid> {
  std::size_t operator()(const mcom::Uuid &uuid) const;
};

}  // namespace std
