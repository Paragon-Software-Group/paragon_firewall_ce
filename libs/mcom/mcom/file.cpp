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

#include "file.hpp"

#include <copyfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>

namespace mcom {

namespace {

File::Attributes MakeAttributes(const struct stat &info) noexcept {
  const auto mode = info.st_mode;
  const File::Type type = (S_ISDIR(mode) ? File::Type::Directory
                                         : S_ISREG(mode) ? File::Type::Regular
                                                         : File::Type::Unknown);

  return {size_t(info.st_size), type};
}

int MakeFlags(File::Flags flags) {
  int res = 0;

  if (flags.bits.write) {
    if (flags.bits.read) {
      res |= O_RDWR;
    } else {
      res |= O_WRONLY;
    }
  }

  if (flags.bits.create) {
    res |= O_CREAT;

    if (flags.bits.excl) {
      res |= O_EXCL;
    }
  }

  return res;
}

}  // namespace

namespace file_details {

FileBuffer::FileBuffer(int fd) : fd_{fd}, buffer_{new char[4096]} {}

FileBuffer::FileBuffer(FileBuffer &&other)
    : fd_{other.fd_}, buffer_{std::move(other.buffer_)} {
  fd_ = -1;
}

FileBuffer::~FileBuffer() {
  if (fd_ != -1) {
    ::close(fd_);
  }
}

int FileBuffer::underflow() {
  auto buffer_base = buffer_.get();

  ssize_t read_res = ::read(fd_, buffer_base, 4096);
  if (read_res <= 0) {
    return traits_type::eof();
  }
  setg(buffer_base, buffer_base, buffer_base + read_res);
  return traits_type::to_int_type(*buffer_base);
}

}  // namespace file_details

IOStream::IOStream(int fd)
    : file_details::FileBuffer{fd}, std::iostream{this} {}

IOStream::IOStream(IOStream &&other)
    : file_details::FileBuffer{std::move(*this)},
      std::iostream{std::move(*this)} {}

File::File(File &&other) : fd_(other.fd_) { other.fd_ = -1; }

File::~File() {
  if (fd_ != -1) {
    ::close(fd_);
  }
}

Result<File> File::Open(const FilePath &path, Flags flags) noexcept {
  int fd;

  if (flags.bits.create) {
    fd = ::open(path.CString(), MakeFlags(flags), flags.mode);
  } else {
    fd = ::open(path.CString(), MakeFlags(flags));
  }

  if (fd == -1) {
    return std::make_error_code(std::errc(errno));
  }

  return File{fd};
}

std::error_code File::Close() noexcept {
  if (0 != ::close(fd_)) {
    return std::make_error_code(std::errc(errno));
  }

  fd_ = -1;

  return {};
}

Result<size_t> File::Read(void *bytes, size_t length) noexcept {
  while (true) {
    ssize_t res = ::read(fd_, bytes, length);
    if (res < 0) {
      if (errno == EINTR) {
        continue;
      }

      return std::make_error_code(std::errc(errno));
    }
    return size_t(res);
  }
}

Result<std::vector<uint8_t>> File::ReadAll() noexcept {
  constexpr size_t buffer_size = 0x100000;
  std::unique_ptr<uint8_t[]> buffer{new uint8_t[buffer_size]};
  std::vector<uint8_t> result;

  while (true) {
    Result<size_t> read_size = Read(buffer.get(), buffer_size);
    if (read_size == 0) {
      break;
    } else if (read_size > 0) {
      result.insert(result.end(), buffer.get(), buffer.get() + *read_size);
    } else if (!read_size) {
      return read_size.Code();
    }
  }

  return std::move(result);
}

std::error_code File::Write(const void *bytes, size_t length) noexcept {
  auto base = static_cast<const uint8_t *>(bytes);
  size_t written = 0;

  while (written < length) {
    ssize_t res = ::write(fd_, base + written, length - written);

    if (res < 0) {
      std::make_error_code(std::errc(errno));
    }

    written += size_t(res);
  }

  return {};
}

Result<File::Attributes> File::GetAttributes() noexcept {
  struct stat info;

  if (0 != ::fstat(fd_, &info)) {
    return std::make_error_code(std::errc(errno));
  }

  return MakeAttributes(info);
}

Result<File::Attributes> File::GetAttributes(const FilePath &path) noexcept {
  struct stat info;

  if (0 != ::lstat(path.CString(), &info)) {
    return std::make_error_code(std::errc(errno));
  }

  return MakeAttributes(info);
}

std::error_code File::Remove(const FilePath &path) noexcept {
  int err = (0 == ::unlink(path.CString())) ? 0 : errno;

  return std::make_error_code(std::errc(err));
}

int File::ExtractDescriptor() && noexcept {
  int fd = fd_;
  fd_ = -1;
  return fd;
}

IOStream File::IoStream() && noexcept {
  int stream_fd = -1;
  std::swap(stream_fd, fd_);
  return IOStream{stream_fd};
}

Result<File> File::OpenAt(int dir_fd, const FilePath &path, File::Flags flags) {
  int fd;

  if (flags.bits.create) {
    fd = ::openat(dir_fd, path.CString(), MakeFlags(flags), flags.mode);
  } else {
    fd = ::openat(dir_fd, path.CString(), MakeFlags(flags));
  }

  if (fd == -1) {
    return std::make_error_code(std::errc(errno));
  }

  return File{fd};
}

Result<std::pair<File, File>> Pipe() noexcept {
  int pipe_fds[2];

  if (0 != ::pipe(pipe_fds)) {
    return std::make_error_code(std::errc(errno));
  }

  return Result<std::pair<File, File>>{in_place,
                                       File::WithDescriptor(pipe_fds[0]),
                                       File::WithDescriptor(pipe_fds[1])};
}

std::error_code CopyFile(const mcom::FilePath &src,
                         const mcom::FilePath &dst) noexcept {
  int status = ::copyfile(
      src.CString(), dst.CString(), nullptr,
      COPYFILE_ALL | COPYFILE_RECURSIVE | COPYFILE_CLONE | COPYFILE_NOFOLLOW);

  if (status) {
    return std::error_code{errno, std::system_category()};
  }

  return {};
}

Result<std::vector<uint8_t>> GetExtendedAttribute(const mcom::FilePath &path,
                                                  const std::string &name) {
  std::vector<uint8_t> value_buffer;

  while (true) {
    ssize_t result =
        ::getxattr(path.CString(), name.c_str(), nullptr, 0, 0, XATTR_NOFOLLOW);
    if (result < 0) {
      return std::error_code{errno, std::system_category()};
    }

    value_buffer.resize(static_cast<size_t>(result));

    result = ::getxattr(path.CString(), name.c_str(), value_buffer.data(),
                        value_buffer.size(), 0, XATTR_NOFOLLOW);

    if (result >= 0) {
      value_buffer.resize(static_cast<size_t>(result));
      break;
    } else if (errno != ERANGE) {
      return std::error_code{errno, std::system_category()};
    }
  }

  return value_buffer;
}

Result<std::map<std::string, std::vector<uint8_t>>> ExtendedAttributesAtPath(
    const FilePath &path) {
  std::vector<char> buffer;

  while (true) {
    ssize_t result = ::listxattr(path.CString(), nullptr, 0, XATTR_NOFOLLOW);
    if (result < 0) {
      return std::error_code{errno, std::system_category()};
    }

    buffer.resize(static_cast<size_t>(result));

    result = ::listxattr(path.CString(), buffer.data(), buffer.size(),
                         XATTR_NOFOLLOW);

    if (result >= 0) {
      buffer.resize(static_cast<size_t>(result));
      break;
    } else if (errno != ERANGE) {
      return std::error_code{errno, std::system_category()};
    }
  }

  std::map<std::string, std::vector<uint8_t>> xattrs;

  auto it = buffer.begin();
  std::vector<char>::iterator null_it;

  while (buffer.end() != (null_it = std::find(it, buffer.end(), '\0'))) {
    std::string name{it, null_it};

    Result<std::vector<uint8_t>> value_buffer =
        GetExtendedAttribute(path, name);
    if (!value_buffer) {
      return value_buffer.Code();
    }

    xattrs.emplace(std::move(name), std::move(*value_buffer));

    it = ++null_it;
  }

  return xattrs;
}

bool CheckFullDiskAccess() {
  FilePath tcc_path{"/Library/Application Support/com.apple.TCC"};

  Result<std::vector<uint8_t>> result =
      GetExtendedAttribute(tcc_path, "com.apple.rootless");
  return result || result.Code() != std::errc::operation_not_permitted;
}

}  // namespace mcom
