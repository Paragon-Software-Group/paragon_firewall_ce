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

#include "file_path.hpp"

namespace mcom {

FilePath::FilePath(std::string_view path) : path_(path) {}

FilePath::FilePath(const FilePath &path) : path_(path.path_) {}

FilePath::FilePath(FilePath &&path) : path_(std::move(path.path_)) {}

FilePath &FilePath::operator=(const FilePath &other) {
  path_ = other.path_;
  return *this;
}

FilePath &FilePath::operator=(FilePath &&path) {
  path_ = std::move(path.path_);
  return *this;
}

FilePath FilePath::Dirname() const noexcept {
  // Find the last separator
  auto sep = path_.find_last_of('/');
  if (sep == std::string::npos) {
    // No separator: path is relative
    return FilePath{"."};
  }

  if (sep == 0) {
    return FilePath{"/"};
  }

  return FilePath{path_.substr(0, sep)};
}

FilePath FilePath::Basename() const noexcept {
  if (path_.empty()) {
    return *this;
  }

  auto begin = path_.cbegin();
  auto end = path_.cend();

  // FIXME: Maybe do it in constructors?
  // Skip trailing separators
  while (end[-1] == '/') {
    end--;
    if (end == begin) {
      return FilePath{"/"};
    }
  }

  // Find basename start
  auto bn_begin = end;
  while (bn_begin != begin && bn_begin[-1] != '/') {
    bn_begin--;
  }

  return FilePath{std::string{bn_begin, end}};
}

std::pair<FilePath, FilePath> FilePath::Split() const noexcept {
  if (path_.empty()) {
    return {FilePath{"."}, *this};
  }

  auto begin = path_.cbegin();
  auto end = path_.cend();

  // FIXME: Maybe do it in constructors?
  // Skip trailing separators
  while (end[-1] == '/') {
    end--;
    if (end == begin) {
      return {FilePath{"/"}, FilePath{"/"}};
    }
  }

  // Find basename start
  auto bn_begin = end;
  while (bn_begin != begin && bn_begin[-1] != '/') {
    bn_begin--;
  }

  auto dirname = bn_begin == begin ? FilePath{"./"}
                                   : FilePath{std::string{begin, bn_begin}};
  return {dirname, FilePath{std::string{bn_begin, end}}};
}

FilePath operator+(const FilePath &path, const char *str) {
  return FilePath{path.String() + str};
}

FilePath operator/(const FilePath &dir, const char *name) {
  if (*name == '/') {
    return FilePath{name};
  } else {
    return FilePath{dir.String() + '/' + name};
  }
}

FilePath operator/(const FilePath &dir, const FilePath &name) {
  if (*name.CString() == '/') {
    return name;
  } else {
    return FilePath{dir.String() + '/' + name.String()};
  }
}

bool operator==(const FilePath &lhs, const FilePath &rhs) {
  return lhs.String() == rhs.String();
}

bool operator!=(const FilePath &lhs, const FilePath &rhs) {
  return lhs.String() != rhs.String();
}

bool operator<(const FilePath &lhs, const FilePath &rhs) {
  return lhs.String() < rhs.String();
}

}  // namespace mcom
