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

#include <CoreFoundation/CoreFoundation.h>
#include <memory>
#include <optional>

#include <mcom/file_path.hpp>
#include <mcom/optional.hpp>

namespace mcom {
namespace cf {

template <class CF>
inline CF RetainSafe(CF cf) {
  return cf ? CF(CFRetain(cf)) : nullptr;
}

inline void ReleaseSafe(CFTypeRef cf) {
  if (cf) {
    CFRelease(cf);
  }
}

class TypePtr {
 public:
  TypePtr() noexcept;

  static TypePtr FromUnretained(CFTypeRef cf) noexcept {
    return {RetainSafe(cf)};
  }

  static TypePtr FromRetained(CFTypeRef cf) noexcept { return {cf}; }

  TypePtr(const TypePtr &ptr) noexcept : cf_{RetainSafe(ptr.cf_)} {}
  TypePtr(TypePtr &&) noexcept;

  operator bool() const noexcept { return cf_ != nullptr; }

  ~TypePtr() { ReleaseSafe(cf_); }

  CFTypeRef Get() const noexcept { return cf_; }

 private:
  TypePtr(CFTypeRef cf) noexcept : cf_{cf} {}

  CFTypeRef cf_;
};

template <class T>
struct TypeFunc;

template <CFTypeID (*F)(void)>
struct TypeFuncImpl {
  static inline CFTypeID TypeID() { return F(); }
};

template <class T, class = mcom::void_t<>>
struct HasTypeFunc : std::false_type {};

template <class T>
struct HasTypeFunc<T, mcom::void_t<decltype(TypeFunc<T>::TypeID())>>
    : std::true_type {};

template <class T>
struct HasTypeFunc<T, mcom::void_t<decltype(T::TypeID())>> : std::true_type {};

template <class T, typename = void>
class PtrBase {
 public:
  PtrBase(T ptr) noexcept : cf_{ptr} {}

  PtrBase(const PtrBase &ptr) noexcept : cf_{RetainSafe(ptr.cf_)} {}

  PtrBase(PtrBase &&ptr) noexcept : cf_(ptr.cf_) { ptr.cf_ = nullptr; }

  ~PtrBase() { ReleaseSafe(cf_); }

  operator bool() const noexcept { return cf_ != nullptr; }

  T get() const noexcept { return cf_; }

  PtrBase &operator=(const PtrBase &other) noexcept {
    ReleaseSafe(cf_);
    cf_ = RetainSafe(other.cf_);
    return *this;
  }

  PtrBase &operator=(PtrBase &&other) noexcept {
    ReleaseSafe(cf_);
    cf_ = other.cf_;
    other.cf_ = nullptr;
    return *this;
  }

 protected:
  T cf_;
};

template <class T, bool>
class PtrImpl;

template <class T>
class PtrImpl<T, false> : public PtrBase<T> {
 public:
  using PtrBase<T>::PtrBase;
};

template <class T>
class PtrImpl<T, true> : public PtrBase<T>, public TypeFunc<T> {
 public:
  using PtrBase<T>::PtrBase;

  PtrImpl(const TypePtr &ptr) : PtrBase<T>{RetainCast(ptr.Get())} {}

 private:
  T RetainCast(CFTypeRef cf) {
    if (cf && CFGetTypeID(cf) == TypeFunc<T>::TypeID()) {
      return static_cast<T>(CFRetain(cf));
    }
    return nullptr;
  }
};

template <class T>
class Ptr : public PtrImpl<T, HasTypeFunc<T>::value> {
 public:
  using PtrImpl<T, HasTypeFunc<T>::value>::PtrImpl;
};

template <>
struct TypeFunc<CFNumberRef> : TypeFuncImpl<CFNumberGetTypeID> {};

class Number : public Ptr<CFNumberRef> {
 public:
  using Ptr<CFNumberRef>::Ptr;

  template <class N>
  Number(N n)
      : Ptr{CFNumberCreate(kCFAllocatorDefault, NumberType<N>::value, &n)} {}

  template <class N>
  N Value() const {
    N value;
    CFNumberGetValue(cf_, NumberType<N>::value, &value);
    return value;
  }

 private:
  template <class N>
  struct NumberType;

  template <class N>
  CFNumberRef Create(N n);
};

#define MCOM_CF_NUMBER_TYPE(type, value) \
  template <>                            \
  struct Number::NumberType<type>        \
      : std::integral_constant<CFNumberType, value> {}

MCOM_CF_NUMBER_TYPE(int, kCFNumberIntType);
MCOM_CF_NUMBER_TYPE(SInt64, kCFNumberSInt64Type);

template <>
struct TypeFunc<CFDictionaryRef> : TypeFuncImpl<CFDictionaryGetTypeID> {};

class Dictionary : public Ptr<CFDictionaryRef> {
 public:
  using Ptr<CFDictionaryRef>::Ptr;

  template <class T = TypePtr>
  auto GetValue(CFTypeRef key) -> std::enable_if_t<!HasTypeFunc<T>::value, T> {
    return TypePtr::FromUnretained(CFDictionaryGetValue(cf_, key));
  }

  template <class T>
  auto GetValue(CFTypeRef key) -> std::enable_if_t<HasTypeFunc<T>::value, T> {
    return TypePtr::FromUnretained(CFDictionaryGetValue(cf_, key));
  }

  static Dictionary Create(
      std::initializer_list<std::pair<CFTypeRef, CFTypeRef>> pairs);
};

class MutableDictionary : public Ptr<CFMutableDictionaryRef> {
 public:
  using Ptr<CFMutableDictionaryRef>::Ptr;

  MutableDictionary(const Dictionary &dict);

  void SetValue(CFTypeRef key, CFTypeRef value);

  void RemoveValue(CFTypeRef key);

  static MutableDictionary Create(CFIndex capacity = 0);
};

template <>
struct TypeFunc<CFArrayRef> : TypeFuncImpl<CFArrayGetTypeID> {};

class Array : public Ptr<CFArrayRef> {
 public:
  using Ptr<CFArrayRef>::Ptr;

  CFIndex GetCount() const;

  template <class T = TypePtr>
  auto GetValueAtIndex(CFIndex index)
      -> std::enable_if_t<!HasTypeFunc<T>::value, T> {
    return TypePtr::FromUnretained(CFArrayGetValueAtIndex(cf_, index));
  }

  template <class T>
  auto GetValueAtIndex(CFIndex index)
      -> std::enable_if_t<HasTypeFunc<T>::value, T> {
    return TypePtr::FromUnretained(CFArrayGetValueAtIndex(cf_, index));
  }

  static Array Create(std::initializer_list<CFTypeRef> values);

  bool ContainsValue(CFTypeRef value) const;
};

class MutableArray : public Ptr<CFMutableArrayRef> {
 public:
  using Ptr<CFMutableArrayRef>::Ptr;

  MutableArray(const Array &arr);

  void InsertValueAtIndex(CFIndex idx, CFTypeRef value);
};

template <>
struct TypeFunc<CFStringRef> : TypeFuncImpl<CFStringGetTypeID> {};

class String : public Ptr<CFStringRef> {
 public:
  using Ptr<CFStringRef>::Ptr;

  std::string GetCString(
      CFStringEncoding encoding = kCFStringEncodingUTF8) const;

  static String WithCString(const char *c_str);
};

class Data;

template <>
struct TypeFunc<CFURLRef> : TypeFuncImpl<CFURLGetTypeID> {};

class URL : public Ptr<CFURLRef> {
 public:
  using Ptr<CFURLRef>::Ptr;

  URL(const FilePath &path, bool is_directory);

  String FileSystemPath() const;

  Optional<FilePath> FileSystemRepresentation(bool resolve_against_base) const;

  Data CreateBookmarkData();
};

template <>
struct TypeFunc<CFBundleRef> : TypeFuncImpl<CFBundleGetTypeID> {};

class Bundle : public Ptr<CFBundleRef> {
 public:
  using Ptr<CFBundleRef>::Ptr;

  Bundle(const URL &url);

  static Bundle GetMain();

  URL ExecutableURL() const;

  URL BundleURL() const;

  Dictionary InfoDictionary() const;
};

template <>
struct TypeFunc<CFDataRef> : TypeFuncImpl<CFDataGetTypeID> {};

class Data : public Ptr<CFDataRef> {
 public:
  using Ptr<CFDataRef>::Ptr;

  const UInt8 *GetBytePtr() const;

  CFIndex GetLength() const;

  void GetBytes(CFRange range, UInt8 *buffer) const;

  static Data WithBytesNoCopy(const UInt8 *bytes, CFIndex length,
                              CFAllocatorRef);

  static Data WithContentsOfFile(const FilePath &path);

  static mcom::Optional<Data> WithBookmarkToURL(
      const URL &url, const URL *relative_to = nullptr);
};

Data Serialize(CFPropertyListRef plist, CFPropertyListFormat format);

TypePtr Deserialize(const Data &data);

template <>
struct TypeFunc<CFDateRef> : TypeFuncImpl<CFDateGetTypeID> {};

class Date : public Ptr<CFDateRef> {
 public:
  using Ptr<CFDateRef>::Ptr;

  explicit Date(CFTimeInterval);

  CFTimeInterval AbsoluteTime() const;

  CFTimeInterval AbsoluteTimeSince1970() const;
};

class Preferences {
 public:
  static mcom::Optional<bool> BooleanValueForKey(CFStringRef key,
                                                 CFStringRef application_id);

  static TypePtr ValueForKey(CFStringRef key, CFStringRef application_id);

  template <class T>
  static void SetValueForKey(CFStringRef key, T &&value,
                             CFStringRef application_id) {
    if constexpr (std::is_pointer_v<std::remove_reference_t<T>> ||
                  std::is_null_pointer_v<T>) {
      CFPreferencesSetAppValue(key, value, application_id);
    } else {
      CFPreferencesSetAppValue(key, value.get(), application_id);
    }
  }
};

}  // namespace cf
}  // namespace mcom
