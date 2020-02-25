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

#include <system_error>
#include <vector>

#include <dirent.h>

#include "mcom/file.hpp"

namespace mcom {

class Directory {
 public:
  Directory(Directory &&);
  ~Directory();

  Result<FilePath> Path() const noexcept;

  static std::error_code Create(const FilePath &path) noexcept;

  static std::error_code CreateWithSubdirectories(
      const FilePath &path) noexcept;

  static Result<Directory> Open(const FilePath &path) noexcept;

  static Result<FilePath> CreateTemporary() noexcept;

  static Result<Directory> OpenTemporary() noexcept;

  static std::error_code Remove(const FilePath &path) noexcept;

  static std::error_code RemoveRecursive(const FilePath &path) noexcept;

  Result<File> OpenEntry(const FilePath &path, File::Flags flags) noexcept;

  Result<Directory> OpenSubdirectory(const FilePath &path) noexcept;

  bool EntryExists(const FilePath &path) noexcept;

  Result<std::vector<FilePath>> Entries() noexcept;

  bool WillRemoveWhenClosed() const noexcept;
  void SetRemoveWhenClosed(bool remove) noexcept;

  std::error_code RemoveEntry(const FilePath &path) noexcept;

  std::error_code RemoveSubdirectory(const FilePath &path) noexcept;

  std::error_code RenameEntry(const FilePath &from_path,
                              const FilePath &to_path) noexcept;

  std::error_code CreateSubdirectory(const FilePath &path) noexcept;

  std::error_code Import(const FilePath &src_path,
                         const FilePath &name) noexcept;

  std::error_code Import(const FilePath &src_path) noexcept;

  std::error_code Close() noexcept;

 private:
  Directory(int fd);

  int fd_;
  DIR *dir_;
  bool remove_ = false;
};

}  // namespace mcom
