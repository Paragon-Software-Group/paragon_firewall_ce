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

#include "directory.hpp"

#include <copyfile.h>
#include <fcntl.h>
#include <os/log.h>
#include <removefile.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mcom {

Directory::Directory(int fd) : fd_(fd), dir_(nullptr) {}

Directory::Directory(Directory &&dir)
    : fd_(dir.fd_), dir_(dir.dir_), remove_(dir.remove_) {
  dir.fd_ = -1;
  dir.dir_ = nullptr;
}

Directory::~Directory() { Close(); }

std::error_code Directory::Create(const FilePath &path) noexcept {
  int err = (0 == ::mkdir(path.CString(), 0755)) ? 0 : errno;

  return std::make_error_code(std::errc(err));
}

std::error_code Directory::CreateWithSubdirectories(
    const FilePath &path) noexcept {
  int err = ::mkpath_np(path.CString(), 0755);

  return std::make_error_code(std::errc(err));
}

Result<Directory> Directory::Open(const FilePath &path) noexcept {
  int fd = ::open(path.CString(), O_RDONLY);

  if (fd == -1) {
    return std::make_error_code(std::errc(errno));
  } else {
    return {fd};
  }
}

Result<FilePath> Directory::CreateTemporary() noexcept {
  char path_template[] = "/tmp/.XXXXXX";

  char *path = ::mkdtemp(path_template);
  if (!path) {
    return std::make_error_code(std::errc(errno));
  }

  return FilePath{path};
}

Result<Directory> Directory::OpenTemporary() noexcept {
  auto path = CreateTemporary();
  if (!path) {
    return path.Code();
  }

  auto dir = Open(*path);
  if (!dir) {
    Remove(*path);
    return dir.Code();
  }

  dir->SetRemoveWhenClosed(true);
  return *std::move(dir);
}

std::error_code Directory::Remove(const FilePath &path) noexcept {
  if (0 != ::rmdir(path.CString())) {
    int err = errno;
    os_log_error(OS_LOG_DEFAULT,
                 "failed to remove directory %s: %{darwin.errno}d",
                 path.CString(), err);
    return std::error_code{err, std::system_category()};
  }

  os_log_debug(OS_LOG_DEFAULT, "did remove directory %s", path.CString());

  return {};
}

std::error_code Directory::RemoveRecursive(const FilePath &path) noexcept {
  if (0 != ::removefile(path.CString(), nullptr, REMOVEFILE_RECURSIVE)) {
    int err = errno;
    os_log_error(OS_LOG_DEFAULT,
                 "removefile recursive failed for %s: %{darwin.errno}d",
                 path.CString(), err);
    return std::error_code{err, std::system_category()};
  }

  os_log_debug(OS_LOG_DEFAULT, "did recursively remove %s", path.CString());

  return {};
}

Result<FilePath> Directory::Path() const noexcept {
  char path_buffer[MAXPATHLEN];

  if (0 != ::fcntl(fd_, F_GETPATH, path_buffer)) {
    return std::make_error_code(std::errc(errno));
  }

  return FilePath{path_buffer};
};

Result<File> Directory::OpenEntry(const FilePath &path,
                                  File::Flags flags) noexcept {
  return File::OpenAt(fd_, path, flags);
}

std::error_code Directory::RenameEntry(const FilePath &from_path,
                                       const FilePath &to_path) noexcept {
  int err = ::renameat(fd_, from_path.CString(), fd_, to_path.CString()) == 0
                ? 0
                : errno;

  return std::make_error_code(std::errc(err));
}

Result<Directory> Directory::OpenSubdirectory(const FilePath &path) noexcept {
  int fd = ::openat(fd_, path.CString(), O_RDONLY);

  if (fd == -1) {
    return std::make_error_code(std::errc(errno));
  } else {
    return {fd};
  }
}

bool Directory::EntryExists(const FilePath &path) noexcept {
  return 0 == ::faccessat(fd_, path.CString(), F_OK, 0);
}

std::error_code Directory::RemoveEntry(const FilePath &path) noexcept {
  int err = ::unlinkat(fd_, path.CString(), 0) == 0 ? 0 : errno;

  return std::make_error_code(std::errc(err));
}

std::error_code Directory::RemoveSubdirectory(const FilePath &path) noexcept {
  int err = (0 == ::unlinkat(fd_, path.CString(), AT_REMOVEDIR)) ? 0 : errno;

  return std::make_error_code(std::errc(err));
}

Result<std::vector<FilePath>> Directory::Entries() noexcept {
  if (dir_) {
    ::rewinddir(dir_);
  } else {
    dir_ = ::fdopendir(fd_);
    if (dir_ == nullptr) {
      return std::make_error_code(std::errc(errno));
    }
  }

  struct dirent ent;
  struct dirent *entp;
  std::vector<FilePath> es;

  while (true) {
    if (0 != ::readdir_r(dir_, &ent, &entp)) {
      return std::make_error_code(std::errc(errno));
    }

    if (entp != &ent) {
      break;
    }

    // Skip '.' and '..' entries
    if (ent.d_name[0] == '.' &&
        (ent.d_name[1] == '\0' ||
         (ent.d_name[1] == '.' && ent.d_name[2] == '\0'))) {
      continue;
    }

    es.emplace_back(std::string(ent.d_name, ent.d_namlen));
  }

  return es;
}

void Directory::SetRemoveWhenClosed(bool remove) noexcept { remove_ = remove; }

std::error_code Directory::CreateSubdirectory(const FilePath &path) noexcept {
  int err = (0 == ::mkdirat(fd_, path.CString(), 0755)) ? 0 : errno;

  return std::make_error_code(std::errc(err));
}

std::error_code Directory::Import(const FilePath &src_path,
                                  const FilePath &name) noexcept {
  auto in_file = File::Open(src_path, File::Flags{}.Read());
  if (!in_file) {
    return in_file.Code();
  }

  auto out_file = OpenEntry(name, File::Flags{}.Write().CreateExcl(0644));
  if (!out_file) {
    return out_file.Code();
  }

  // TODO: remove out file on copy error

  if (0 != ::fcopyfile(in_file->Descriptor(), out_file->Descriptor(), nullptr,
                       COPYFILE_ALL)) {
    return std::error_code{errno, std::system_category()};
  }

  return {};
}

std::error_code Directory::Import(const FilePath &src_path) noexcept {
  return Import(src_path, src_path.Basename());
}

std::error_code Directory::Close() noexcept {
  if (remove_ && fd_ != -1) {
    if (auto path = this->Path()) {
      RemoveRecursive(*path);
    }
  }

  int err = 0;

  if (dir_ != nullptr) {
    if (0 != ::closedir(dir_)) {
      err = errno;
    }
    dir_ = nullptr;
  } else if (fd_ != -1) {
    if (0 != ::close(fd_)) {
      err = errno;
    }
    fd_ = -1;
  }

  return std::error_code{err, std::system_category()};
}

}  // namespace mcom
