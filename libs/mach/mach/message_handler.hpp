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

#include <cstdlib>
#include <functional>

#include <mach/mach.h>

#include <mcom/optional.hpp>
#include <mcom/result.hpp>

#include "mach/message.hpp"

namespace mach {

template <class... Args>
class Promise {
 public:
  using Handler = std::function<void(Args...)>;

  Promise(Handler handler)
      : used_{std::make_shared<std::atomic<bool>>(false)},
        handler_{std::make_shared<Handler>(std::move(handler))} {}

  void operator()(Args... args) {
    if (used_->exchange(true)) {
      return;
    }
    handler_->operator()(std::forward<Args>(args)...);
    handler_.reset();
  }

  using Types = mcom::types<Args...>;

 private:
  std::shared_ptr<std::atomic<bool>> used_;
  std::shared_ptr<Handler> handler_;
};

template <class T>
struct is_promise_ : std::false_type {};

template <class... Args>
struct is_promise_<Promise<Args...>> : std::true_type {};

template <class T>
inline constexpr bool IsPromise = is_promise_<T>::value;

template <class In, class Out, bool Token>
struct ArgsTraits;

template <class... InArgs, class... OutArgs, bool Token>
struct ArgsTraits<mcom::types<InArgs...>, mcom::types<OutArgs...>, Token> {
  static constexpr bool NeedToken = Token;
  static constexpr bool HasCompletion = true;
  using In = mcom::types<InArgs...>;
  using Out = mcom::types<OutArgs...>;
};

template <class... InArgs, bool Token>
struct ArgsTraits<mcom::types<InArgs...>, void, Token> {
  static constexpr bool NeedToken = Token;
  static constexpr bool HasCompletion = false;
  using In = mcom::types<InArgs...>;
};

template <class... Args>
auto MakeTraitsHelper(mcom::types<Args...>) -> decltype(auto) {
  if constexpr (mcom::is_callable_v<mcom::last_t<Args...>>) {
    if constexpr (std::is_same_v<mcom::AuditToken,
                                 std::decay_t<mcom::type_at_t<-2, Args...>>>) {
      return ArgsTraits<mcom::drop_last_t<2, mcom::types<Args...>>,
                        mcom::func_args_t<mcom::last_t<Args...>>, true>{};
    } else {
      return ArgsTraits<mcom::drop_last_t<1, mcom::types<Args...>>,
                        mcom::func_args_t<mcom::last_t<Args...>>, false>{};
    }
  } else if constexpr (std::is_same_v<mcom::AuditToken,
                                      std::decay_t<mcom::last_t<Args...>>>) {
    return ArgsTraits<mcom::drop_last_t<1, mcom::types<Args...>>, void, true>{};
  } else {
    return ArgsTraits<mcom::types<Args...>, void, false>{};
  }
}

template <class ArgsList>
using MakeTraits = decltype(MakeTraitsHelper(std::declval<ArgsList>()));

class MessageHandler {
 public:
  using Handler = std::function<bool(MessageBuffer &)>;

  template <class Fn>
  static MessageHandler Create(mach_msg_id_t msg_id, Fn &&handler) {
    return std::make_from_tuple<MessageHandler>(
        CreateHandler(msg_id, std::forward<Fn>(handler)));
  }

  using HandlerInfo = std::pair<size_t, Handler>;

  MessageHandler(size_t size, Handler handler)
      : size_{size}, handler_{std::move(handler)} {}

  MessageHandler(MessageHandler &&other)
      : size_{other.size_}, handler_{std::move(other.handler_)} {}

  MessageHandler &operator=(MessageHandler &&) = delete;

  size_t MessageSize() const { return size_; }

  bool Handle(MessageBuffer &buffer) const { return handler_(buffer); }

 private:
  template <class Fn>
  static HandlerInfo CreateHandler(mach_msg_id_t msg_id, Fn &&handler) {
    using Traits = MakeTraits<mcom::func_args_t<Fn>>;

    if constexpr (Traits::HasCompletion) {
      return CreateHandlerWithCompletion(Traits{}, msg_id,
                                         std::forward<Fn>(handler));
    } else {
      return CreateNoReplyHandler(Traits{}, msg_id, std::forward<Fn>(handler));
    }
  }

  template <class... In, class... Out, bool NeedToken, class Fn>
  static HandlerInfo CreateHandlerWithCompletion(
      ArgsTraits<mcom::types<In...>, mcom::types<Out...>, NeedToken>,
      mach_msg_id_t msg_id, Fn &&fn) {
    auto handler = [msg_id, fn = std::forward<Fn>(fn)](MessageBuffer &buffer) {
      // Check the input message id
      if (buffer.MessageId() != msg_id) {
        return false;
      }

      // Check that the message contains a send-once reply port
      auto reply_port = buffer.ExtractReplyPort();
      if (!reply_port) {
        return false;
      }

      // Check & unpack message contents
      auto in_args = buffer.Unpack<In...>();
      if (!in_args) {
        return false;
      }

      auto reply_port_sh =
          std::make_shared<SendOnceRight>(std::move(*reply_port));

      // Construct a reply promise
      Promise<Out...> promise{
          [id = msg_id + 100, port = reply_port_sh](Out... args) {
            Send(id, std::move(*port), std::forward<Out>(args)...);
          }};

      if constexpr (NeedToken) {
        ApplyFn(fn, std::move(in_args.Value()), buffer.AuditToken(),
                std::move(promise));
      } else {
        ApplyFn(fn, std::move(in_args.Value()), std::move(promise));
      }

      return true;
    };

    return {sizeof(Message<In...>), std::move(handler)};
  }

  template <class Fn, class... In, class... Args>
  static void ApplyFn(const Fn &fn, std::tuple<In...> in_args,
                      Args &&... args) {
    std::apply(fn,
               std::tuple_cat(std::move(in_args),
                              std::make_tuple(std::forward<Args>(args)...)));
  }

  template <class... In, bool NeedToken, class Fn>
  static HandlerInfo CreateNoReplyHandler(
      ArgsTraits<mcom::types<In...>, void, NeedToken>, mach_msg_id_t msg_id,
      Fn &&fn) {
    auto handler = [msg_id, fn = std::move(fn)](MessageBuffer &buffer) {
      // Check the input message id
      if (buffer.MessageId() != msg_id) {
        return false;
      }

      // Check & unpack message contents
      auto args = buffer.Unpack<In...>();
      if (!args) {
        return false;
      }

      if constexpr (NeedToken) {
        ApplyFn(fn, std::move(args.Value()), buffer.AuditToken());
      } else {
        ApplyFn(fn, std::move(args.Value()));
      }

      return true;
    };

    return {sizeof(Message<In...>), std::move(handler)};
  }

  const size_t size_;
  Handler handler_;
};

}  // namespace mach
