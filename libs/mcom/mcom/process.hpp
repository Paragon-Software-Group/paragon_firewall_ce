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

#include <vector>

#include <spawn.h>

#include <mcom/file.hpp>
#include <mcom/result.hpp>

namespace mcom {

class FileActions {
 public:
  FileActions();
  FileActions(const FileActions &) = delete;
  FileActions(FileActions &&);

  void AddDup2(int fd1, int fd2);

  posix_spawn_file_actions_t *Get() const noexcept;

  ~FileActions();

 private:
  mutable posix_spawn_file_actions_t actions_;
};

Result<pid_t> SpawnProcess(const char *prog,
                           const std::vector<std::string> &args,
                           const FileActions &actions = {}) noexcept;

std::error_code WaitForProcess(pid_t pid);

Result<FilePath> ProcessPath(pid_t pid);

}  // namespace mcom
