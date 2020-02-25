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

#include <iostream>
#include <map>
#include <system_error>
#include <vector>

#include <mcom/file_path.hpp>
#include <mcom/optional.hpp>
#include <mcom/result.hpp>

namespace mcom {

namespace file_details {

class FileBuffer : public std::streambuf {
 public:
  FileBuffer(int fd);
  FileBuffer(FileBuffer &&);
  FileBuffer &operator=(FileBuffer &&);
  ~FileBuffer();

 protected:
  int underflow() override;

 private:
  int fd_;
  std::unique_ptr<char[]> buffer_;
};

}  // namespace file_details

class IOStream : protected file_details::FileBuffer, public std::iostream {
 public:
  IOStream(const IOStream &) = delete;
  IOStream(IOStream &&);

  IOStream &operator=(IOStream &&) = default;

 private:
  friend class File;

  IOStream(int fd);
};

class File {
 public:
  struct Flags {
    Flags() : bits{0} {}

    inline Flags Read() const noexcept {
      auto flags = *this;
      flags.bits.read = true;
      return flags;
    }

    inline Flags Write() const noexcept {
      auto flags = *this;
      flags.bits.write = true;
      return flags;
    }

    Flags Create(int mode) const noexcept {
      auto flags = *this;
      flags.bits.create = true;
      flags.mode = mode;
      return flags;
    }

    inline Flags CreateExcl(int mode) const noexcept {
      auto flags = Create(mode);
      flags.bits.excl = true;
      return flags;
    }

    Flags DeleteWhenClosed() const;

    struct {
      bool read : 1;
      bool write : 1;
      bool create : 1;
      bool excl : 1;
      bool delete_when_closed : 1;
    } bits;

    int mode;
  };

  enum class Type { Unknown, Regular, Directory };

  struct Attributes {
    size_t size;
    Type type;
  };

  File(const FilePath &path, Flags flags);
  File(const File &) = delete;
  File(File &&);
  ~File();

  static Result<File> Open(const FilePath &path, Flags flags) noexcept;

  std::error_code Close() noexcept;

  Result<size_t> Read(void *bytes, size_t length) noexcept;

  Result<std::vector<uint8_t>> ReadAll() noexcept;

  std::error_code Write(const void *bytes, size_t length) noexcept;

  Result<Attributes> GetAttributes() noexcept;

  static Result<Attributes> GetAttributes(const FilePath &path) noexcept;
  static std::error_code Remove(const FilePath &path) noexcept;

  static File WithDescriptor(int fd) noexcept { return File(fd); }

  int Descriptor() const noexcept { return fd_; }

  int ExtractDescriptor() && noexcept;

  IOStream IoStream() && noexcept;

 private:
  File(int fd) : fd_(fd) {}

  friend class Directory;
  static Result<File> OpenAt(int dir_fd, const FilePath &path, Flags flags);

  int fd_;
};

Result<std::pair<File, File>> Pipe() noexcept;

std::error_code CopyFile(const mcom::FilePath &src,
                         const mcom::FilePath &dst) noexcept;

Result<std::vector<uint8_t>> GetExtendedAttribute(const mcom::FilePath &path,
                                                  const std::string &name);

Result<std::map<std::string, std::vector<uint8_t>>> ExtendedAttributesAtPath(
    const FilePath &path);

bool CheckFullDiskAccess(void);

}  // namespace mcom
