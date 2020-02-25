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

#include "mcom/cf.hpp"

#include <sys/param.h>

#include "mcom/file.hpp"

namespace mcom {
namespace cf {

Dictionary Dictionary::Create(
    std::initializer_list<std::pair<CFTypeRef, CFTypeRef>> pairs) {
  const CFIndex count = pairs.size();

  std::unique_ptr<CFTypeRef[]> keys(new CFTypeRef[count]);
  std::unique_ptr<CFTypeRef[]> vals(new CFTypeRef[count]);

  CFIndex index = 0;
  for (const auto &pair : pairs) {
    keys[index] = pair.first;
    vals[index] = pair.second;
    index += 1;
  }

  return CFDictionaryCreate(kCFAllocatorDefault, keys.get(), vals.get(), count,
                            &kCFTypeDictionaryKeyCallBacks,
                            &kCFTypeDictionaryValueCallBacks);
}

void MutableDictionary::SetValue(CFTypeRef key, CFTypeRef value) {
  CFDictionarySetValue(cf_, key, value);
}

void MutableDictionary::RemoveValue(CFTypeRef key) {
  CFDictionaryRemoveValue(cf_, key);
}

MutableDictionary::MutableDictionary(const Dictionary &dict)
    : Ptr{CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dict.get())} {}

MutableDictionary MutableDictionary::Create(CFIndex capacity) {
  return CFDictionaryCreateMutable(kCFAllocatorDefault, capacity,
                                   &kCFTypeDictionaryKeyCallBacks,
                                   &kCFTypeDictionaryValueCallBacks);
}

CFIndex Array::GetCount() const { return CFArrayGetCount(cf_); }

Array Array::Create(std::initializer_list<CFTypeRef> values) {
  std::unique_ptr<CFTypeRef[]> vals{new CFTypeRef[values.size()]};

  std::copy(values.begin(), values.end(), vals.get());

  return CFArrayCreate(kCFAllocatorDefault, vals.get(), values.size(),
                       &kCFTypeArrayCallBacks);
}

bool Array::ContainsValue(CFTypeRef value) const {
  return CFArrayContainsValue(cf_, CFRangeMake(0, GetCount()), value);
}

MutableArray::MutableArray(const Array &arr)
    : Ptr{CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, arr.get())} {}

void MutableArray::InsertValueAtIndex(CFIndex idx, CFTypeRef value) {
  CFArrayInsertValueAtIndex(cf_, idx, value);
}

std::string String::GetCString(CFStringEncoding encoding) const {
  if (auto ptr = CFStringGetCStringPtr(cf_, encoding)) {
    return ptr;
  }

  const auto length = CFStringGetLength(cf_);

  CFIndex used_buf_len = 0;

  const auto count =
      CFStringGetBytes(cf_, {.length = length, .location = 0}, encoding, 0,
                       false, nullptr, 0, &used_buf_len);
  if (!count) {
    return {};
  }

  std::string str;
  str.resize(used_buf_len);

  CFStringGetBytes(cf_, {.length = length, .location = 0}, encoding, 0, false,
                   reinterpret_cast<UInt8 *>(&str.front()), used_buf_len,
                   nullptr);

  return str;
}

String String::WithCString(const char *c_str) {
  return CFStringCreateWithCString(kCFAllocatorDefault, c_str,
                                   kCFStringEncodingUTF8);
}

URL::URL(const FilePath &path, bool is_directory)
    : Ptr{CFURLCreateFromFileSystemRepresentation(
          kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(path.CString()),
          std::strlen(path.CString()), is_directory)} {}

String URL::FileSystemPath() const {
  return CFURLCopyFileSystemPath(cf_, kCFURLPOSIXPathStyle);
}

Optional<FilePath> URL::FileSystemRepresentation(
    bool resolve_against_base) const {
  UInt8 buffer[MAXPATHLEN + 1];

  if (CFURLGetFileSystemRepresentation(cf_, resolve_against_base, buffer,
                                       MAXPATHLEN)) {
    return FilePath{reinterpret_cast<char *>(buffer)};
  }

  return nullopt;
}

Data URL::CreateBookmarkData() {
  return CFURLCreateBookmarkData(kCFAllocatorDefault, cf_, 0, nullptr, nullptr,
                                 nullptr);
}

Bundle::Bundle(const URL &url)
    : Ptr{CFBundleCreate(kCFAllocatorDefault, url.get())} {}

Bundle Bundle::GetMain() { return Bundle{RetainSafe(CFBundleGetMainBundle())}; }

URL Bundle::ExecutableURL() const { return CFBundleCopyExecutableURL(cf_); }

URL Bundle::BundleURL() const { return CFBundleCopyBundleURL(cf_); }

Dictionary Bundle::InfoDictionary() const {
  return RetainSafe(CFBundleGetInfoDictionary(cf_));
}

const UInt8 *Data::GetBytePtr() const { return CFDataGetBytePtr(cf_); }

CFIndex Data::GetLength() const { return CFDataGetLength(cf_); }

void Data::GetBytes(CFRange range, UInt8 *buffer) const {
  CFDataGetBytes(cf_, range, buffer);
}

Data Data::WithBytesNoCopy(const UInt8 *bytes, CFIndex length,
                           CFAllocatorRef allocator) {
  return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, bytes, length,
                                     allocator);
}

Data Data::WithContentsOfFile(const FilePath &path) {
  if (auto file = File::Open(path, File::Flags{}.Read())) {
    if (auto attrs = file->GetAttributes()) {
      std::unique_ptr<void, void (*)(void *)> buffer{std::malloc(attrs->size),
                                                     std::free};
      if (file->Read(buffer.get(), attrs->size) == attrs->size) {
        auto bytes = static_cast<UInt8 *>(buffer.release());
        return CFDataCreateWithBytesNoCopy(kCFAllocatorMalloc, bytes,
                                           attrs->size, kCFAllocatorMalloc);
      }
    }
  }

  return nullptr;
}

Optional<Data> Data::WithBookmarkToURL(const mcom::cf::URL &url,
                                       const URL *relative_to) {
  CFErrorRef error;
  CFURLRef relative = relative_to ? relative_to->get() : NULL;
  auto bookmark_data = CFURLCreateBookmarkData(
      kCFAllocatorDefault, url.get(), kCFURLBookmarkCreationMinimalBookmarkMask,
      NULL, relative, &error);
  if (bookmark_data) {
    return Data(bookmark_data);
  }
  return nullopt;
}

Data Serialize(CFPropertyListRef plist, CFPropertyListFormat format) {
  return CFPropertyListCreateData(kCFAllocatorDefault, plist, format, 0,
                                  nullptr);
}

TypePtr Deserialize(const Data &data) {
  return TypePtr::FromRetained(
      CFPropertyListCreateWithData(kCFAllocatorDefault, data.get(),
                                   kCFPropertyListImmutable, nullptr, nullptr));
}

Date::Date(CFTimeInterval at) : Ptr{CFDateCreate(kCFAllocatorDefault, at)} {}

CFTimeInterval Date::AbsoluteTime() const { return CFDateGetAbsoluteTime(cf_); }

CFTimeInterval Date::AbsoluteTimeSince1970() const {
  return AbsoluteTime() + kCFAbsoluteTimeIntervalSince1970;
}

mcom::Optional<bool> Preferences::BooleanValueForKey(
    CFStringRef key, CFStringRef application_id) {
  Boolean valid = FALSE;
  Boolean value = CFPreferencesGetAppBooleanValue(key, application_id, &valid);
  if (valid == FALSE) {
    return mcom::nullopt;
  }
  return (value != FALSE);
}

TypePtr Preferences::ValueForKey(CFStringRef key, CFStringRef application_id) {
  return TypePtr::FromRetained(CFPreferencesCopyAppValue(key, application_id));
}

}  // namespace cf
}  // namespace mcom
