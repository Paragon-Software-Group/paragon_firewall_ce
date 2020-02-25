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

#include <optional>
#include <variant>
#include <vector>

#include <dispatch/dispatch.h>
#include <mach/mach.h>

#include <mcom/dispatch.hpp>
#include <mcom/sync.hpp>

#include "mach/message_handler.hpp"
#include "mach/server_internal.hpp"

namespace mach {

class Server {
 public:
  struct GlobalQueue {};
  struct MainQueue {};

  static constexpr GlobalQueue concurrent_queue{};
  static constexpr MainQueue main_queue{};
  using QueueType = std::variant<GlobalQueue, MainQueue>;

  Server(const ReceiveRight &port, QueueType queue = concurrent_queue);

  ~Server();

  Server &operator=(Server &&) = delete;

  template <class Fn>
  void AddHandler(mach_msg_id_t msg_id, Fn &&handler);

  void AddHandler(MessageHandler &&handler);

  void Suspend();

  void Resume();

  void Cancel();

  const ReceiveRight &Port() const { return port_; }

  template <class Fn>
  void RequestNoSendersNotification(Fn &&fn);

 private:
  struct Handlers {
    std::vector<MessageHandler> general;
    std::optional<MessageHandler> no_senders;
  };

  bool UsesMainQueue() const {
    return std::holds_alternative<MainQueue>(queue_type_);
  }

  static bool IsOnMainQueue();

  void HandleSourceEvent() const;

  void HandleMessage(MessageBuffer &buffer) const;

  template <class Fn>
  auto AccessHandlers(Fn &&fn) -> decltype(fn(std::declval<Handlers &>()));

  template <class Fn>
  auto AccessHandlers(Fn &&fn) const
      -> decltype(fn(std::declval<const Handlers &>()));

  const ReceiveRight &port_;
  dispatch::Queue queue_;
  const QueueType queue_type_;
  dispatch::MachReceiveSource source_;
  dispatch::Semaphore source_cancellation_semaphore_{0};
  dispatch::Group group_;
  mcom::Sync<Handlers> handlers_;
};

template <class Fn>
void Server::AddHandler(mach_msg_id_t msg_id, Fn &&handler) {
  AccessHandlers([&](Handlers &handlers) {
    if (UsesMainQueue()) {
      handlers.general.emplace_back(
          MessageHandler::Create(msg_id, std::forward<Fn>(handler)));
    } else {
      handlers.general.emplace_back(MessageHandler::Create(
          msg_id, server_internal::AsyncHandlerFor<Fn>{
                      std::forward<Fn>(handler), group_, queue_}));
    }
  });
}

template <class Fn>
void Server::RequestNoSendersNotification(Fn &&fn) {
  struct NoSendersInfo {
    NDR_record_t ndr;
    mach_msg_type_number_t not_count;
  };

  AccessHandlers([&](Handlers &handlers) {
    handlers.no_senders.emplace(MessageHandler::Create(
        MACH_NOTIFY_NO_SENDERS,
        [fn = std::forward<Fn>(fn)](NoSendersInfo) { fn(); }));
  });

  mach_port_t previous = MACH_PORT_NULL;
  mach_port_request_notification(mach_task_self(), Port().Name(),
                                 MACH_NOTIFY_NO_SENDERS, 0, Port().Name(),
                                 MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);

  if (previous) {
    SendOnceRight::Construct(previous).Invalidate();
  }
}

template <class Fn>
auto Server::AccessHandlers(Fn &&fn)
    -> decltype(fn(std::declval<Handlers &>())) {
  if (UsesMainQueue()) {
    if (IsOnMainQueue()) {
      return fn(handlers_.AccessUnsafely());
    } else {
      return queue_.Sync([&]() { return fn(handlers_.AccessUnsafely()); });
    }
  } else {
    return handlers_.Use(std::forward<Fn>(fn));
  }
}

template <class Fn>
auto Server::AccessHandlers(Fn &&fn) const
    -> decltype(fn(std::declval<const Handlers &>())) {
  if (UsesMainQueue()) {
    if (IsOnMainQueue()) {
      return fn(handlers_.AccessUnsafely());
    } else {
      return queue_.Sync([&]() { return fn(handlers_.AccessUnsafely()); });
    }
  } else {
    return handlers_.Use(std::forward<Fn>(fn));
  }
}

}  // namespace mach
