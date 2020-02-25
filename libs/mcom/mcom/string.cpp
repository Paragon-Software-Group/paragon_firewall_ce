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

#include "string.hpp"

#include <cerrno>
#include <system_error>

namespace mcom {

std::vector<std::string> StrSplit(const std::string &str, char delimiter) {
  size_t pos = 0;
  size_t dpos;
  std::vector<std::string> comps;

  while ((dpos = str.find_first_of(delimiter, pos)) != str.npos) {
    comps.push_back(str.substr(pos, dpos - pos));
    pos = dpos + 1;
  }

  return comps;
}

std::string StrFormat(const char *format, ...) {
  va_list vl;
  va_start(vl, format);
  std::string str = StrFormat(format, vl);
  va_end(vl);

  return str;
}

std::string StrFormat(const char *format, va_list vl) {
  char *s = nullptr;

  int err = (::vasprintf(&s, format, vl) >= 0) ? 0 : errno;

  if (err != 0) {
    throw std::system_error{std::make_error_code(std::errc(err))};
  }

  std::string ss{s};
  free(s);
  return ss;
}

bool HasSuffix(const char *str, const char *suffix) {
  const size_t str_len = std::strlen(str);
  const size_t suffix_len = std::strlen(suffix);

  if (str_len < suffix_len) {
    return false;
  }

  return 0 == std::memcmp(str + str_len - suffix_len, suffix, suffix_len);
}

}  // namespace mcom
