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

#include "server.hpp"

#include <cstdio>

namespace mach {

namespace {

template <class Variant>
struct VariantSize;

template <class... Ts>
struct VariantSize<std::variant<Ts...>>
    : std::integral_constant<size_t, sizeof...(Ts)> {};

template <class... Fns>
struct Visitor : Fns... {
  using Fns::operator()...;

  template <size_t Idx, class Variant>
  auto Visit(Variant &&var) const {
    if constexpr (Idx == VariantSize<std::decay_t<Variant>>::value - 1) {
      return this->operator()(*std::get_if<Idx>(&var));
    } else {
      if (var.index() == Idx) {
        return this->operator()(*std::get_if<Idx>(&var));
      } else {
        return Visit<Idx + 1>(std::forward<Variant>(var));
      }
    }
  }
};

template <class Variant, class... Fns>
auto Visit(Variant &&var, Fns &&... fns) {
  return Visitor<Fns...>{std::forward<Fns>(fns)...}.template Visit<0>(
      std::forward<Variant>(var));
}

const int kMainQueueSpecificKey = 0;
int kMainQueueSpecificValue = 0;

dispatch::Queue QueueForType(Server::QueueType type) {
  return Visit(
      type,
      [](Server::MainQueue) {
        MCOM_ONCE([]() {
          dispatch_queue_set_specific(dispatch_get_main_queue(),
                                      &kMainQueueSpecificKey,
                                      &kMainQueueSpecificValue, nullptr);
        });
        return dispatch::Queue::Main();
      },
      [](Server::GlobalQueue) { return dispatch::Queue{}; });
}

}  // namespace

Server::Server(const ReceiveRight &port, QueueType queue)
    : port_{port},
      queue_{QueueForType(queue)},
      queue_type_{queue},
      source_{port.Name(), queue_} {
  source_.SetEventHandler([this]() { HandleSourceEvent(); });

  source_.SetCancelHandler(
      [this]() { source_cancellation_semaphore_.Signal(); });
}

Server::~Server() { Cancel(); }

void Server::AddHandler(MessageHandler &&handler) {
  AccessHandlers([&](Handlers &handlers) {
    handlers.general.emplace_back(std::move(handler));
  });
}

void Server::Resume() { source_.Resume(); }

void Server::Suspend() { source_.Suspend(); }

void Server::Cancel() {
  // Cancel dispatch source and wait until it has finished it's work.
  source_.Cancel();
  source_cancellation_semaphore_.Wait(dispatch::Time::kForever);

  // Wait for all handlers to finish
  group_.Wait(dispatch::Time::kForever);
}

bool Server::IsOnMainQueue() {
  return &kMainQueueSpecificValue ==
         dispatch_queue_get_specific(dispatch_get_main_queue(),
                                     &kMainQueueSpecificKey);
}

void Server::HandleSourceEvent() const {
  size_t max_message_size = 0;

  AccessHandlers([&](const Handlers &handlers) {
    for (auto &handler : handlers.general) {
      max_message_size = std::max(max_message_size, handler.MessageSize());
    }
  });

  auto buffer = MessageBuffer::Receive(port_, max_message_size);
  if (!buffer) {
    return;
  }

  if (UsesMainQueue()) {
    HandleMessage(*buffer);
  } else {
    group_.Async(queue_, [this, buffer = std::move(*buffer)]() mutable {
      HandleMessage(buffer);
    });
  }
}

void Server::HandleMessage(MessageBuffer &buffer) const {
  const bool handled = AccessHandlers([&](const Handlers &handlers) {
    bool handled = std::any_of(
        handlers.general.begin(), handlers.general.end(),
        [&buffer](auto &handler) { return handler.Handle(buffer); });

    if (!handled && buffer.MessageId() == MACH_NOTIFY_NO_SENDERS &&
        handlers.no_senders) {
      handled = handlers.no_senders->Handle(buffer);
    }

    return handled;
  });

  if (!handled) {
    std::fprintf(stderr, "no handler for msg(%d)\n", buffer.MessageId());
  }
}

}  // namespace mach
