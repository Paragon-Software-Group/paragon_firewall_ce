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

#include "mcom/process.hpp"

#include <libproc.h>

namespace {

void ArgvFill(const char *prog, const std::vector<std::string> &args,
              char **argv) {
  *argv++ = ::strdup(prog);
  for (const auto &str : args) {
    *argv++ = ::strdup(str.c_str());
  }
  *argv = nullptr;
}

void ArgvRelease(char **argv) {
  while (*argv) {
    std::free(*argv++);
  }
}

}  // namespace

namespace mcom {

FileActions::FileActions() { posix_spawn_file_actions_init(&actions_); }

FileActions::~FileActions() { posix_spawn_file_actions_destroy(&actions_); }

void FileActions::AddDup2(int fd1, int fd2) {
  posix_spawn_file_actions_adddup2(&actions_, fd1, fd2);
}

posix_spawn_file_actions_t *FileActions::Get() const noexcept {
  return &actions_;
}

Result<pid_t> SpawnProcess(const char *prog,
                           const std::vector<std::string> &args,
                           const FileActions &actions) noexcept {
  char *argv[args.size() + 2];

  ArgvFill(prog, args, argv);

  pid_t pid;
  int status = posix_spawn(&pid, prog, actions.Get(), nullptr, argv, nullptr);

  ArgvRelease(argv);

  if (status != 0) {
    return std::make_error_code(std::errc(status));
  }

  return pid;
}

std::error_code WaitForProcess(pid_t pid) {
  int status = 0;

  while (true) {
    if (-1 == ::waitpid(pid, &status, 0)) {
      if (errno == EINTR) {
        continue;
      }
      return std::error_code{errno, std::system_category()};
    }
    break;
  }

  if (WIFSIGNALED(status)) {
    return std::error_code{WTERMSIG(status), std::system_category()};
  }

  return {WEXITSTATUS(status), std::system_category()};
}

Result<FilePath> ProcessPath(pid_t pid) {
  if (pid <= 0) {
    return std::error_code{EINVAL, std::system_category()};
  }

  char buffer[PROC_PIDPATHINFO_MAXSIZE];
  auto result = proc_pidpath(pid, buffer, sizeof(buffer));
  if (result <= 0) {
    return std::error_code{errno, std::system_category()};
  }

  return mcom::FilePath{buffer};
}

}  // namespace mcom
