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
//
// Created by Alexey Antipov on 18/02/2019.
//

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <mcom/optional.hpp>

#include <mach/mach.h>

#include <mach/message.hpp>

namespace mach {

template <class T, class = void>
struct Codable;

class Encoder {
 public:
  template <class T>
  Encoder &Encode(const T &value) {
    Codable<T>{}.Encode(*this, value);
    return *this;
  }

  Encoder &EncodeInt32(int32_t value) { return EncodeTrivial(value); }

  Encoder &EncodeInt64(int64_t value) { return EncodeTrivial(value); }

  Encoder &EncodeDouble(double value) { return EncodeTrivial(value); }

  Encoder &EncodeString(std::string_view str) {
    EncodeInt32(static_cast<int32_t>(str.size()));
    return AddBytes(str.data(), str.size());
  }

  template <class T>
  Encoder &EncodeTrivial(const T &value) {
    return AddBytes(&value, sizeof(value));
  }

  mach_msg_ool_descriptor_t CopyDescriptor() const;

  Encoder &AddBytes(const void *bytes, size_t size);

 private:
  size_t Align(size_t size) { return (size + 3) & ~3; }

  std::vector<uint8_t> buffer_;
};

class Decoder {
 public:
  Decoder(mach_msg_ool_descriptor_t &descriptor)
      : address_{descriptor.address}, size_{descriptor.size} {
    descriptor.size = 0;
  }

  ~Decoder() {
    if (size_ != 0) {
      ::vm_deallocate(mach_task_self(),
                      reinterpret_cast<vm_address_t>(address_),
                      static_cast<vm_size_t>(size_));
    }
  }

  int32_t DecodeInt32() { return DecodeTrivial<int32_t>(); }

  std::string_view DecodeString();

  Decoder &operator=(Decoder &&) = delete;

  template <class T>
  T DecodeTrivial() {
    return *static_cast<const T *>(DecodeBytes(sizeof(T)));
  }

 private:
  const void *DecodeBytes(size_t size);

  size_t Align(size_t size) { return (size + 3) & ~3; }

  const void *address_;
  size_t size_;
};

template <class T, class = void>
struct is_mach_codable : std::false_type {};

template <class T>
struct is_mach_codable<T, std::void_t<decltype(&Codable<T>::Encode)>>
    : std::true_type {};

template <class T>
constexpr inline bool is_mach_codable_v = is_mach_codable<T>::value;

template <>
struct Codable<std::string> {
  void Encode(Encoder &encoder, std::string_view string) {
    encoder.EncodeString(string);
  }

  std::string Decode(Decoder &decoder) {
    return std::string{decoder.DecodeString()};
  }
};

template <>
struct Codable<std::string_view> {
  void Encode(Encoder &encoder, std::string_view string) {
    encoder.EncodeString(string);
  }
};

template <class T>
struct Codable<std::vector<T>, std::enable_if_t<is_mach_codable_v<T>>> {
  void Encode(Encoder &encoder, const std::vector<T> &items) {
    encoder.EncodeInt32(static_cast<int32_t>(items.size()));

    for (auto &item : items) {
      encoder.Encode(item);
    }
  }

  std::vector<T> Decode(Decoder &decoder) {
    auto size = decoder.DecodeInt32();
    std::vector<T> result;
    for (auto i = 0; i < size; ++i) {
      result.emplace_back(Codable<T>{}.Decode(decoder));
    }
    return result;
  }
};

template <class T>
struct Codable<mcom::Optional<T>, std::enable_if_t<is_mach_codable_v<T>>> {
  void Encode(Encoder &encoder, const mcom::Optional<T> &item) {
    encoder.EncodeInt32(item ? 1 : 0);

    if (item) {
      encoder.Encode(*item);
    }
  }
};

template <class T>
struct Codable<std::optional<T>, std::enable_if_t<is_mach_codable_v<T>>> {
  void Encode(Encoder &encoder, const std::optional<T> &item) {
    encoder.EncodeInt32(item ? 1 : 0);

    if (item) {
      encoder.Encode(*item);
    }
  }

  std::optional<T> Decode(Decoder &decoder) {
    if (decoder.DecodeInt32() == 0) {
      return std::nullopt;
    }
    return Codable<T>{}.Decode(decoder);
  }
};

template <class... Ts>
struct Codable<
    std::variant<Ts...>,
    std::enable_if_t<((is_mach_codable_v<Ts> || std::is_trivial_v<Ts>)&&...)>> {
  using Variant = std::variant<Ts...>;

  void Encode(Encoder &encoder, const Variant &var) {
    encoder.EncodeInt32(static_cast<int32_t>(var.index()));

    EncodeImpl(encoder, var, std::index_sequence_for<Ts...>{});
  }

  Variant Decode(Decoder &decoder) {
    const auto index = static_cast<size_t>(decoder.DecodeInt32());

    std::unique_ptr<Variant> var_ptr;

    DecodeImpl(decoder, index, var_ptr, std::index_sequence_for<Ts...>{});

    return std::move(*var_ptr);
  }

 private:
  template <size_t... Is>
  static void EncodeImpl(Encoder &encoder, const Variant &var,
                         std::index_sequence<Is...>) {
    (EncodeAlternative<Is, Ts>(encoder, var) || ...);
  }

  template <size_t Index, class T>
  static bool EncodeAlternative(Encoder &encoder, const Variant &var) {
    if (var.index() != Index) {
      return false;
    }

    if constexpr (std::is_trivial_v<T>) {
      encoder.EncodeTrivial(*std::get_if<Index>(&var));
    } else {
      encoder.Encode(*std::get_if<Index>(&var));
    }

    return true;
  }

  template <size_t... Is>
  static void DecodeImpl(Decoder &decoder, size_t index,
                         std::unique_ptr<Variant> &var_ptr,
                         std::index_sequence<Is...>) {
    (DecodeAlternative<Is, Ts>(decoder, index, var_ptr) || ...);
  }

  template <size_t Index, class T>
  static bool DecodeAlternative(Decoder &decoder, size_t index,
                                std::unique_ptr<Variant> &var_ptr) {
    if (index != Index) {
      return false;
    }

    if constexpr (std::is_trivial_v<T>) {
      var_ptr = std::make_unique<Variant>(std::in_place_index<Index>,
                                          decoder.DecodeTrivial<T>());
    } else {
      var_ptr = std::make_unique<Variant>(std::in_place_index<Index>,
                                          Codable<T>{}.Decode(decoder));
    }

    return true;
  }
};

template <class T>
struct message_codable<
    T, std::enable_if_t<is_mach_codable_v<T> && !mcom::is_optional_v<T>>> {
  T unpack(mach_msg_ool_descriptor_t &descriptor) {
    Decoder decoder{descriptor};
    return Codable<T>{}.Decode(decoder);
  }

  mach_msg_ool_descriptor_t pack(const T &value) {
    Encoder encoder;
    encoder.Encode(value);
    return encoder.CopyDescriptor();
  }
};

}  // namespace mach
