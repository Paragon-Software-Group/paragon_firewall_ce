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

#include <string>

namespace mcom {

class FilePath {
 public:
  explicit FilePath(std::string_view path);
  FilePath(const FilePath &);
  FilePath(FilePath &&);

  FilePath &operator=(FilePath &&);
  FilePath &operator=(const FilePath &);

  const char *CString() const { return path_.c_str(); }
  const std::string &String() const { return path_; }

  FilePath Dirname() const noexcept;
  FilePath Basename() const noexcept;
  std::vector<FilePath> Components() const noexcept;
  std::pair<FilePath, FilePath> Split() const noexcept;

 private:
  std::string path_;
};

FilePath operator+(const FilePath &path, const char *str);
FilePath operator/(const FilePath &dir, const FilePath &name);
FilePath operator/(const FilePath &dir, const char *str);

bool operator==(const FilePath &lhs, const FilePath &rhs);
bool operator!=(const FilePath &lhs, const FilePath &rhs);
bool operator<(const FilePath &, const FilePath &);

}  // namespace mcom
