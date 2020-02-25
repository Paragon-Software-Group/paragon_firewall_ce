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

#include <mcom/disk_name.hpp>

#include <mcom/string.hpp>

namespace mcom {

std::optional<DiskName> DiskName::FromString(const std::string &str) {
  unsigned major, minor;
  int consumed;
  if (2 == sscanf(str.c_str(), "disk%us%u%n", &major, &minor, &consumed) &&
      consumed == str.size()) {
    return DiskName{int(major), int(minor)};
  } else if (1 == sscanf(str.c_str(), "disk%u%n", &major, &consumed) &&
             consumed == str.size()) {
    return DiskName{int(major), -1};
  }
  return std::nullopt;
}

std::string DiskName::ToString() const {
  return (minor_ == -1) ? StrFormat("disk%d", major_)
                        : StrFormat("disk%ds%d", major_, minor_);
}

DiskName::DiskName(int major, const std::optional<int> &minor)
    : DiskName{major, minor.value_or(-1)} {}

int DiskName::Major() const { return major_; }

std::optional<int> DiskName::Minor() const {
  if (minor_ < 0) {
    return std::nullopt;
  }
  return minor_;
}

}  // namespace mcom
