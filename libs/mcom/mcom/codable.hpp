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
#include <vector>

#include <mcom/optional.hpp>
#include <mcom/utility.hpp>

namespace mcom {

template <class T>
struct codable;

template <class, class = void_t<>>
struct is_codable : std::false_type {};

template <class T>
static constexpr bool is_codable_v = is_codable<T>::value;

class Encoder {
 public:
  Encoder() {}

  const std::vector<uint8_t> &Bytes() { return bytes_; }

  template <class T>
  auto EncodePod(const T &value) -> std::enable_if_t<std::is_pod<T>::value> {
    auto begin = reinterpret_cast<const uint8_t *>(std::addressof(value));
    bytes_.insert(bytes_.end(), begin, begin + sizeof(T));
  }

  void EncodeString(const char *str);

  void EncodeString(const std::string &str) {
    EncodePod<size_t>(str.size());
    auto begin = reinterpret_cast<const uint8_t *>(str.data());
    bytes_.insert(bytes_.end(), begin, begin + str.size());
  }

  template <class T>
  auto Encode(const T &value) -> std::enable_if_t<is_codable_v<T>> {
    codable<T>{}.encode(value, *this);
  }

 private:
  std::vector<uint8_t> bytes_;
};

class Decoder {
 public:
  Decoder(const std::vector<uint8_t> &bytes)
      : bytes_{bytes}, ptr_{bytes_.data()} {}

  template <class T>
  auto DecodePod() -> std::enable_if_t<std::is_pod<T>::value, T> {
    auto value_ptr = reinterpret_cast<const T *>(ptr_);
    ptr_ += sizeof(T);
    return *value_ptr;
  }

  std::string DecodeString() {
    const auto size = DecodePod<size_t>();
    auto begin = reinterpret_cast<const char *>(ptr_);
    ptr_ += size;
    return std::string{begin, begin + size};
  }

  template <class T>
  auto Decode() -> std::enable_if_t<is_codable_v<T>, T> {
    return codable<T>{}.decode(*this);
  }

 private:
  const std::vector<uint8_t> bytes_;
  const uint8_t *ptr_;
};

template <class T>
struct is_codable<
    T, void_t<std::enable_if_t<
                  std::is_same<T, decltype(std::declval<codable<T>>().decode(
                                      std::declval<Decoder &>()))>::value>,
              void_t<decltype(std::declval<codable<T>>().encode(
                  std::declval<const T &>(), std::declval<Encoder &>()))>>>
    : std::true_type {};

namespace details {

template <class T, class = void_t<>>
struct vector_codable_impl {};

template <class T>
struct vector_codable_impl<T, void_t<std::enable_if_t<is_codable_v<T>>>> {
  void encode(const std::vector<T> &vector, Encoder &encoder) {
    encoder.EncodePod<size_t>(vector.size());
    for (const auto &item : vector) {
      encoder.Encode(item);
    }
  }

  std::vector<T> decode(Decoder &decoder) {
    const std::size_t size = decoder.DecodePod<size_t>();
    std::vector<T> vector;
    for (size_t i = 0; i < size; ++i) {
      vector.emplace_back(decoder.Decode<T>());
    }
    return vector;
  }
};

}  // namespace details

template <class T>
struct codable<std::vector<T>> : details::vector_codable_impl<T> {};

template <>
struct codable<std::string> {
  void encode(const std::string &str, Encoder &encoder) {
    encoder.EncodeString(str);
  }

  std::string decode(Decoder &decoder) { return decoder.DecodeString(); }
};

}  // namespace mcom
