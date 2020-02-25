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
// Created by Alexey Antipov on 13/02/2019.
//

#pragma once

#include <cstdlib>
#include <system_error>
#include <utility>

#include <mach/mach.h>

#include <mcom/dispatch.hpp>
#include <mcom/optional.hpp>
#include <mcom/result.hpp>
#include <mcom/security.hpp>
#include <mcom/types.hpp>

#include "mach/port.hpp"

namespace mach {

const std::error_category &error_category() noexcept;

template <class T, class = void>
struct message_codable;

template <class T>
using pack_result_t =
    decltype(std::declval<message_codable<T>>().pack(std::declval<T>()));

template <class R, class T, class U>
U unpack_arg_helper(R (T::*)(U));

template <class T, class = void>
using unpack_arg_type_t =
    decltype(unpack_arg_helper(&message_codable<T>::unpack));

template <class T, class = void>
struct has_unpack : std::false_type {};

template <class T>
struct has_unpack<T, std::void_t<unpack_arg_type_t<T>>> : std::true_type {};

template <class T, class = void>
struct has_pack : std::false_type {};

template <class T>
struct has_pack<T, std::void_t<pack_result_t<T>>> : std::true_type {};

template <class T, bool HasPack = false>
struct descriptor_type_for {
  using type = unpack_arg_type_t<T>;
};

template <class T>
struct descriptor_type_for<T, true> {
  using type = pack_result_t<T>;
};

template <class T>
using descriptor_type_for_t =
    std::decay_t<typename descriptor_type_for<T, has_pack<T>::value>::type>;

template <class T>
inline constexpr bool is_complex_v = has_unpack<T>::value || has_pack<T>::value;

template <class T, bool HasPack = false>
struct pack_type_s {
  using type = const T &;
};

template <class T>
struct pack_type_s<T, true> {
  using type =
      decltype(unpack_arg_helper(&message_codable<std::decay_t<T>>::pack)) &&;
};

template <class T>
using pack_type_t = typename pack_type_s<T, has_pack<T>::value>::type;

template <class... Ts>
inline constexpr bool has_complex_v = (is_complex_v<Ts> || ...);

template <class T>
struct plain_data {
  static_assert(std::is_trivial_v<T>);
  static_assert(!is_complex_v<T>);

  T value;
};

template <size_t I, class T>
struct body_elt_t {
  descriptor_type_for_t<T> descriptor;
};

template <size_t I, class T, bool Complex>
struct body_elt_for_s {
  struct type {};
};

template <size_t I, class T>
struct body_elt_for_s<I, T, true> {
  using type = body_elt_t<I, T>;
};

template <size_t I, class T>
using body_elt_for_t = typename body_elt_for_s<I, T, is_complex_v<T>>::type;

template <class... Ts>
struct complex_body_t;

template <size_t... Is, class... Ts>
struct complex_body_t<std::index_sequence<Is...>, Ts...>
    : mach_msg_body_t, body_elt_for_t<Is, Ts>... {};

template <bool Complex, class... Ts>
struct body_for_s {
  struct type {};
};

template <class... Ts>
struct body_for_s<true, Ts...> {
  using type = complex_body_t<std::index_sequence_for<Ts...>, Ts...>;
};

template <class... Ts>
using body_for_t = typename body_for_s<has_complex_v<Ts...>, Ts...>::type;

template <size_t I, class T>
struct data_elt_t {
  static_assert(std::is_trivial_v<T>);

  T value;
};

template <size_t I, class T, bool Complex>
struct data_elt_for_s {
  struct type {};
};

template <size_t I, class T>
struct data_elt_for_s<I, T, false> {
  using type = data_elt_t<I, T>;
};

template <size_t I, class T>
using data_elt_for_t = typename data_elt_for_s<I, T, is_complex_v<T>>::type;

struct MessageHeader {
  mach_msg_header_t header;
};

struct NullRight {
  constexpr mach_port_name_t Name() const { return MACH_PORT_NULL; }
};

struct Null {
  const NullRight right;

  static constexpr mach_msg_type_name_t disposition = 0;
};

template <class Right>
struct Move;

template <class Right>
struct Copy;

template <class Right>
struct Make;

template <>
struct Move<ReceiveRight> {
  ReceiveRight right;

  static constexpr mach_msg_type_name_t disposition =
      MACH_MSG_TYPE_MOVE_RECEIVE;
};

template <>
struct Move<SendRight> {
  SendRight right;

  static constexpr mach_msg_type_name_t disposition = MACH_MSG_TYPE_MOVE_SEND;
};

template <>
struct Move<SendOnceRight> {
  SendOnceRight right;

  static constexpr mach_msg_type_name_t disposition =
      MACH_MSG_TYPE_MOVE_SEND_ONCE;
};

template <class Right>
Move(Right &&right)->Move<std::decay_t<Right>>;

template <>
struct Copy<SendRight> {
  const SendRight &right;

  static constexpr mach_msg_type_name_t disposition = MACH_MSG_TYPE_COPY_SEND;
};

Copy(const SendRight &)->Copy<SendRight>;

template <>
struct Make<SendRight> {
  const ReceiveRight &right;

  static constexpr mach_msg_type_name_t disposition = MACH_MSG_TYPE_MAKE_SEND;
};

template <>
struct Make<SendOnceRight> {
  const ReceiveRight &right;

  static constexpr mach_msg_type_name_t disposition =
      MACH_MSG_TYPE_MAKE_SEND_ONCE;
};

using MakeSendOnce = Make<SendOnceRight>;
using MakeSend = Make<SendRight>;

template <class Right>
struct IsMoved : std::false_type {};

template <class Right>
struct IsMoved<Move<Right>> : std::true_type {};

template <class Is, class... Ts>
struct MessageImpl;

namespace message_details {

template <class... Args>
auto MessageTypeHelper(mcom::types<Args...>) {
  return MessageImpl<std::index_sequence_for<Args...>, Args...>{};
}

template <bool WithAuditToken, class... Args>
auto MessageTypeTokenHelper() {
  if constexpr (WithAuditToken) {
    return MessageTypeHelper(mcom::drop_last_t<1, mcom::types<Args...>>{});
  } else {
    return MessageTypeHelper(mcom::types<Args...>{});
  }
}

}  // namespace message_details

template <size_t... Is, class... Ts>
struct MessageImpl<std::index_sequence<Is...>, Ts...>
    : MessageHeader, body_for_t<Ts...>, data_elt_for_t<Is, Ts>... {
  static constexpr bool is_complex = has_complex_v<Ts...>;
  static constexpr bool plain_data_size =
      (sizeof(data_elt_for_t<Is, Ts>) + ... + 0);

  mach_msg_header_t &Header() { return header; }

  bool Check() {
    if (header.msgh_size != sizeof(MessageImpl)) {
      return false;
    }

    if (is_complex != ((header.msgh_bits & MACH_MSGH_BITS_COMPLEX) != 0)) {
      return false;
    }

    if constexpr (is_complex) {
      if (static_cast<mach_msg_body_t *>(this)->msgh_descriptor_count !=
          ((is_complex_v<Ts> ? 1 : 0) + ...)) {
        return false;
      }
    }

    return true;
  }

  template <class SendRight>
  static std::error_code Send(mach_msg_id_t msg_id, SendRight &&send_right,
                              pack_type_t<Ts>... args) {
    return Send(msg_id, std::forward<SendRight>(send_right), Null{},
                std::forward<pack_type_t<Ts>>(args)...);
  }

  template <class SendRight, class ReplyRight>
  static std::error_code Send(mach_msg_id_t msg_id, SendRight &&send_right,
                              ReplyRight &&reply_right,
                              pack_type_t<Ts>... args) {
    MessageImpl message;
    message.Pack(std::forward<pack_type_t<Ts>>(args)...);

    auto &header = message.header;
    header.msgh_id = msg_id;
    header.msgh_size = sizeof(MessageImpl);
    header.msgh_remote_port = send_right.right.Name();
    header.msgh_local_port = reply_right.right.Name();
    header.msgh_bits = SendRight::disposition | (ReplyRight::disposition << 8);

    if constexpr (is_complex) {
      header.msgh_bits |= MACH_MSGH_BITS_COMPLEX;

      static_cast<body_for_t<Ts...> *>(&message)->msgh_descriptor_count =
          ((is_complex_v<Ts> ? 1 : 0) + ...);
    }

    const auto status =
        ::mach_msg(&header, MACH_SEND_MSG, header.msgh_size, 0, MACH_PORT_NULL,
                   MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

    if (status == MACH_SEND_INVALID_DEST) {
      // Destroy the message
      (message.DestroyElt<Is, Ts>(), ...);
    } else {
      if constexpr (IsMoved<std::decay_t<SendRight>>::value) {
        std::move(send_right.right).Extract();
      }
      if constexpr (IsMoved<std::decay_t<ReplyRight>>::value) {
        std::move(reply_right.right).Extract();
      }
    }

    return {status, error_category()};
  }

  template <class Result, class SendRight>
  static mcom::Result<Result> SendReceive(mach_msg_id_t msg_id,
                                          SendRight &&send_right,
                                          pack_type_t<Ts>... args) {
    if constexpr (mcom::is_tuple_v<Result>) {
      return SendReceiveImpl<NeedTokenForArgs(mcom::tuple_types_t<Result>{})>(
          mcom::tuple_types_t<Result>{}, msg_id,
          std::forward<SendRight>(send_right),
          std::forward<pack_type_t<Ts>>(args)...);
    } else {
      mcom::Result<std::tuple<Result>> result =
          SendReceiveImpl<std::is_same_v<Result, mcom::AuditToken>>(
              mcom::types<Result>{}, msg_id,
              std::forward<SendRight>(send_right),
              std::forward<pack_type_t<Ts>>(args)...);
      if (result) {
        return std::get<0>(std::move(*result));
      } else {
        return result.Code();
      }
    }
  }

  template <class... OutArgs>
  static inline constexpr bool NeedTokenForArgs(mcom::types<OutArgs...>) {
    if constexpr (sizeof...(OutArgs) != 0) {
      if constexpr (std::is_same_v<mcom::last_t<OutArgs...>,
                                   mcom::AuditToken>) {
        return true;
      }
    }
    return false;
  }

  template <bool WithAuditToken, class SendRight, class... OutArgs>
  static mcom::Result<std::tuple<OutArgs...>> SendReceiveImpl(
      mcom::types<OutArgs...>, mach_msg_id_t msg_id, SendRight &&send_right,
      pack_type_t<Ts>... args) {
    using ReceiveMessage = decltype(
        message_details::MessageTypeTokenHelper<WithAuditToken, OutArgs...>());

    union {
      MessageImpl send;
      struct {
        ReceiveMessage msg;
        mach_msg_audit_trailer_t trailer;
      } rcv;
    } message;

    message.send.Pack(std::forward<pack_type_t<Ts>>(args)...);

    auto &header = message.send.header;
    header.msgh_id = msg_id;
    header.msgh_size = sizeof(MessageImpl);
    header.msgh_remote_port = send_right.right.Name();
    header.msgh_local_port = mig_get_reply_port();
    header.msgh_bits =
        SendRight::disposition | (MACH_MSG_TYPE_MAKE_SEND_ONCE << 8);

    if constexpr (is_complex) {
      header.msgh_bits |= MACH_MSGH_BITS_COMPLEX;

      static_cast<mach_msg_body_t *>(&message.send)->msgh_descriptor_count =
          ((is_complex_v<Ts> ? 1 : 0) + ...);
    }

    const auto status = ::mach_msg(
        &header,
        MACH_SEND_MSG | MACH_RCV_MSG |
            MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
            MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT),
        header.msgh_size, sizeof(message.rcv), header.msgh_local_port,
        MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

    switch (status) {
      case MACH_SEND_INVALID_DATA:
      case MACH_SEND_INVALID_DEST:
      case MACH_SEND_INVALID_HEADER:
        mig_put_reply_port(header.msgh_local_port);

      default:
        mig_dealloc_reply_port(header.msgh_local_port);
    }

    if (status == MACH_SEND_INVALID_DEST) {
      // Destroy the message
      (message.send.template DestroyElt<Is, Ts>(), ...);
    } else {
      if constexpr (IsMoved<std::decay_t<SendRight>>::value) {
        std::move(send_right.right).Extract();
      }
    }

    std::error_code error;

    if (status != MACH_MSG_SUCCESS) {
      error = std::error_code{status, error_category()};
    }

    if (message.rcv.msg.header.msgh_id != msg_id + 100) {
      error = std::error_code{MIG_REPLY_MISMATCH, error_category()};
    }

    if (!message.rcv.msg.Check()) {
      error = std::error_code{MIG_TYPE_ERROR, error_category()};
    }

    if (error) {
      mach_msg_destroy(&message.rcv.msg.header);
      return error;
    }

    if constexpr (WithAuditToken) {
      return std::tuple_cat(
          message.rcv.msg.Unpack(),
          std::tuple{mcom::AuditToken{message.rcv.trailer.msgh_audit}});
    } else {
      return message.rcv.msg.Unpack();
    }
  }

  void Pack(pack_type_t<Ts>... args) {
    (PackElt<Is, __type_pack_element<Is, Ts...>>(
         std::forward<pack_type_t<Ts>>(args)),
     ...);
  }

  std::tuple<Ts...> Unpack() { return {UnpackElt<Is, Ts>()...}; }

 private:
  template <size_t I, class T>
  auto PackElt(pack_type_t<T> arg) -> std::enable_if_t<is_complex_v<T>> {
    static_cast<body_elt_t<I, T> *>(this)->descriptor =
        message_codable<T>{}.pack(std::forward<pack_type_t<T>>(arg));
  }

  template <size_t I, class T>
  auto PackElt(pack_type_t<T> arg) -> std::enable_if_t<!is_complex_v<T>> {
    static_cast<data_elt_t<I, T> *>(this)->value = arg;
  }

  template <size_t I, class T>
  auto UnpackElt() -> std::enable_if_t<is_complex_v<T>, T> {
    return message_codable<T>{}.unpack(
        static_cast<body_elt_t<I, T> &>(*this).descriptor);
  }

  template <size_t I, class T>
  auto UnpackElt() -> std::enable_if_t<!is_complex_v<T>, T> {
    return static_cast<data_elt_t<I, T> &>(*this).value;
  }

  template <size_t I, class T>
  void DestroyElt() {
    if constexpr (is_complex_v<T>) {
      Destroy(static_cast<body_elt_t<I, T> &>(*this).descriptor);
    }
  }

  void Destroy(mach_msg_port_descriptor_t &desc) {
    switch (desc.disposition) {
      case MACH_MSG_TYPE_MOVE_SEND:
      case MACH_MSG_TYPE_MOVE_SEND_ONCE:
        ::mach_port_deallocate(mach_task_self(), desc.name);
        break;

      case MACH_MSG_TYPE_MOVE_RECEIVE:
        ::mach_port_mod_refs(mach_task_self(), desc.name,
                             MACH_PORT_RIGHT_RECEIVE, -1);
        break;
    }
  }

  void Destroy(mach_msg_ool_descriptor_t &desc) {
    if (desc.size != 0) {
      ::vm_deallocate(mach_task_self(),
                      reinterpret_cast<vm_address_t>(desc.address), desc.size);
    }
  }
};

template <class... Ts>
struct Message : MessageImpl<std::index_sequence_for<Ts...>, Ts...> {};

class MessageBuffer {
 public:
  static mcom::Result<MessageBuffer> Receive(const ReceiveRight &right,
                                             size_t size);

  MessageBuffer(MessageBuffer &&) = default;

  MessageBuffer &operator=(MessageBuffer &&) = delete;

  ~MessageBuffer();

  mach_msg_id_t MessageId() const;

  mcom::AuditToken AuditToken() const;

  mcom::Optional<SendOnceRight> ExtractReplyPort();

  template <class... Ts>
  mcom::Optional<std::tuple<Ts...>> Unpack() {
    using MessageType = Message<Ts...>;
    auto &message = *static_cast<MessageType *>(buffer_.get());
    if (!message.Check()) {
      return mcom::nullopt;
    }
    return message.Unpack();
  }

 private:
  using Buffer = std::unique_ptr<void, void (*)(void *)>;

  MessageBuffer(Buffer buffer);

  mach_msg_header_t &Header();

  const mach_msg_header_t &Header() const;

  Buffer buffer_;
};

template <bool Extra = false>
struct error_data {
  int value;
};

template <>
struct error_data<true> {
  int value;
  int extra = 0;
};

template <class Msg, bool Complex, size_t PlainDataSize>
struct error_type_for_message {
  using type = error_data<>;
};

template <class Msg>
struct error_type_for_message<Msg, false, sizeof(plain_data<int>)> {
  using type = error_data<true>;
};

template <class Msg>
using error_type_for_message_t =
    typename error_type_for_message<Msg, Msg::is_complex,
                                    Msg::plain_data_size>::type;

template <class... Ts, class Right>
std::error_code SendErrorReply(mach_msg_id_t msg_id, Right &&send_right,
                               int errc) {
  using error_data_type = error_type_for_message_t<Message<Ts...>>;

  return Send(msg_id, std::forward<Right>(send_right), error_data_type{errc});
};

template <class Type>
struct port_codable_base {
  mach_msg_port_descriptor_t pack(Type port) {
    mach_msg_port_descriptor_t desc{};
    desc.type = MACH_MSG_PORT_DESCRIPTOR;
    if constexpr (std::is_reference_v<decltype(port.right)>) {
      desc.name = port.right.Name();
    } else {
      desc.name = std::move(port.right).Extract();
    }
    desc.disposition = Type::disposition;
    return desc;
  }
};

template <class Right>
struct message_codable<Move<Right>> : port_codable_base<Move<Right>> {};

template <class Right>
struct message_codable<Copy<Right>> : port_codable_base<Copy<Right>> {};

template <class Right>
struct message_codable<Make<Right>> : port_codable_base<Make<Right>> {};

template <>
struct message_codable<SendRight> {
  SendRight unpack(mach_msg_port_descriptor_t &descriptor) {
    const auto name = descriptor.name;
    descriptor.name = MACH_PORT_NULL;
    return SendRight::Construct(name);
  }
};

namespace message_details {

template <class SendRight>
struct SendType_;

template <>
struct SendType_<SendRight> {
  using type = Move<SendRight>;
};

template <>
struct SendType_<SendRight &> {
  using type = Copy<SendRight>;
};

template <>
struct SendType_<SendOnceRight> {
  using type = Move<SendOnceRight>;
};

template <class T>
struct RemoveCV_ : std::remove_cv<T> {};

template <class T>
struct RemoveCV_<T &> {
  using type = std::remove_cv_t<T> &;
};

template <class SendRight>
using SendType = typename SendType_<typename RemoveCV_<SendRight>::type>::type;

template <class...>
struct SendHelper;

template <class... In, class... Out>
struct SendHelper<mcom::types<In...>, mcom::types<Out...>> {
  using InTuple = mcom::MakeTupleType<mcom::types<In &&...>>;

  template <class Right, class Completion>
  std::error_code operator()(mach_msg_id_t msg_id, Right &&send_right,
                             InTuple args, Completion &&completion) {
    using ReplyType = mach::Message<std::decay_t<Out>...>;

    auto port = ReceiveRight::Allocate();

    mach_port_t prev = 0;
    auto status = mach_port_request_notification(
        mach_task_self(), port.Name(), MACH_NOTIFY_NO_SENDERS, 1, port.Name(),
        MACH_MSG_TYPE_MAKE_SEND_ONCE, &prev);
    assert(status == 0);
    assert(prev == 0);

    std::error_code error = std::apply(
        Message<std::decay_t<In>...>::template Send<SendType<Right>,
                                                    Make<SendOnceRight>>,
        std::tuple_cat(
            std::forward_as_tuple(
                msg_id, SendType<Right>{std::forward<Right>(send_right)},
                Make<SendOnceRight>{port}),
            std::forward<InTuple>(args)));
    if (error) {
      return error;
    }

    dispatch::Queue{}.Async(
        [reply_id = msg_id + 100, port = std::move(port),
         completion = std::forward<Completion>(completion)]() mutable {
          auto reply = mach::MessageBuffer::Receive(port, sizeof(ReplyType));
          if (!reply) {
            return;
          }

          if (reply->MessageId() != reply_id) {
            return;
          }

          auto result = reply->Unpack<Out...>();
          if (!result) {
            return;
          }

          std::apply(std::forward<Completion>(completion), std::move(*result));
        });

    return error;
  }
};

template <class T>
constexpr T make_empty_descriptor() {
  if constexpr (std::is_same_v<T, mach_msg_port_descriptor_t>) {
    mach_msg_port_descriptor_t descriptor{0};
    descriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    return descriptor;
  } else {
    mach_msg_ool_descriptor_t descriptor{0};
    descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    return descriptor;
  }
}

template <class T>
struct optional_codable_unpack_base {
  using Descriptor = descriptor_type_for_t<T>;

  std::optional<T> unpack(Descriptor &descriptor) {
    if constexpr (std::is_same_v<Descriptor, mach_msg_port_descriptor_t>) {
      if (descriptor.name == 0) {
        return std::nullopt;
      }
    } else {
      if (descriptor.address == nullptr || descriptor.size == 0) {
        return std::nullopt;
      }
    }

    return message_codable<T>{}.unpack(descriptor);
  }
};

template <class T, bool ConstRef = true>
struct optional_codable_pack_base {
  descriptor_type_for_t<T> pack(const std::optional<T> &value) {
    return value ? message_codable<T>{}.pack(*value)
                 : make_empty_descriptor<descriptor_type_for_t<T>>();
  }
};

template <class T>
struct optional_codable_pack_base<T, false> {
  descriptor_type_for_t<T> pack(std::optional<T> value) {
    return value ? message_codable<T>{}.pack(std::move(*value))
                 : make_empty_descriptor<descriptor_type_for_t<T>>();
  }
};

template <class T, bool HasUnpack = true, bool HasPack = true>
struct optional_codable
    : optional_codable_unpack_base<T>,
      optional_codable_pack_base<T, std::is_const_v<T> &&
                                        std::is_lvalue_reference_v<T>> {};

}  // namespace message_details

template <class Right, class... Args>
std::error_code Send(mach_msg_id_t msg_id, Right &&send_right,
                     Args &&... args) {
  if constexpr (mcom::is_callable_v<mcom::last_t<Args...>>) {
    return message_details::SendHelper<
        mcom::drop_last_t<1, mcom::types<Args...>>,
        mcom::func_args_t<mcom::last_t<Args...>>>{}(
        msg_id, std::forward<Right>(send_right),
        mcom::DropLast<1>(std::forward_as_tuple(std::forward<Args>(args)...)),
        std::forward<mcom::last_t<Args...>>(
            mcom::Last(std::forward<Args>(args)...)));
  } else {
    return Message<std::decay_t<Args>...>::Send(
        msg_id,
        message_details::SendType<Right>{std::forward<Right>(send_right)},
        std::forward<Args>(args)...);
  }
}

// Usage examples:
//
// clang-format off
// mcom::Result<int> r0 =
//     mach::SendReceive<int>(msg_id, send_right, arg1, arg2);
//
// mcom::Result<std::tuple<int, mach::SendRight>> r1 =
//     mach::SendReceive<int, mach::SendRight>(msg_id, send_right, …);
//
// mcom::Result<std::tuple<std::optional<SomeCodable>, mcom::AuditToken>> r2 =
//     mach::SendReceive<std::optional<SomeCodable>, mcom::AuditToken>(
//         msg_id, send_right, …);
// clang-format on
//
template <class Result, class Right, class... Args>
mcom::Result<Result> SendReceive(mach_msg_id_t msg_id, Right &&send_right,
                                 Args &&... args) {
  return Message<std::decay_t<Args>...>::template SendReceive<Result>(
      msg_id, message_details::SendType<Right>{std::forward<Right>(send_right)},
      std::forward<pack_type_t<std::decay_t<Args>>>(args)...);
}

template <class T>
struct message_codable<std::optional<T>, std::enable_if_t<is_complex_v<T>>>
    : message_details::optional_codable<T, has_unpack<T>::value,
                                        has_pack<T>::value> {};

}  // namespace mach
