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
// Created by Alexey Antipov on 03.10.2019.
//

#include <os/log.h>

#include <mach/bootstrap.hpp>
#include <mach/coding.hpp>
#include <mach/message.hpp>
#include <mach/server.hpp>

#include "BundleCache.hpp"
#include "extension.hpp"

namespace mach {

template <>
struct Codable<nf::Application> {
  void Encode(Encoder &encoder, const nf::Application &application) {
    encoder.EncodeString(application.Path());
  }

  nf::Application Decode(Decoder &decoder) { return {decoder.DecodeString()}; }
};

template <>
struct Codable<nf::Time> {
  void Encode(Encoder &encoder, const nf::Time &time) {
    encoder.EncodeTrivial(nf::Time::clock::to_time_t(time));
  }

  nf::Time Decode(Decoder &decoder) {
    return nf::Time::clock::from_time_t(decoder.DecodeTrivial<std::time_t>());
  }
};

template <>
struct Codable<nf::Rule> {
  void Encode(Encoder &encoder, const nf::Rule &rule) {
    encoder.EncodeTrivial(rule.Id());
    encoder.EncodeTrivial(rule.Permission());
    encoder.Encode(rule.Application());
    encoder.Encode(rule.LastAccessTime());
    encoder.EncodeTrivial(rule.AccessCount());
  }

  nf::Rule Decode(Decoder &decoder) {
    return nf::Rule{decoder.DecodeTrivial<nf::RuleId>(),
                    decoder.DecodeTrivial<nf::RulePermission>(),
                    Codable<nf::Application>{}.Decode(decoder),
                    Codable<std::optional<nf::Time>>{}.Decode(decoder),
                    decoder.DecodeTrivial<uint64_t>()};
  }
};

template <>
struct Codable<nf::Packet> {
  void Encode(Encoder &encoder, const nf::Packet &packet) {
    encoder.EncodeTrivial(packet.Size());
    encoder.EncodeTrivial(packet.PacketDirection());
    encoder.Encode(packet.Application());
    encoder.Encode(packet.Time());
  }
};

template <>
struct Codable<nf::PacketList> {
  void Encode(Encoder &encoder, const nf::PacketList &packets) {
    auto &storage = packets.Storage();

    encoder.EncodeInt32(static_cast<int32_t>(storage.size()));

    for (auto &list : packets.Storage()) {
      encoder.Encode(list.first);

      auto &info = list.second;
      encoder.EncodeInt32(static_cast<int32_t>(info.size()));
      encoder.AddBytes(
          info.data(),
          reinterpret_cast<const uint8_t *>(info.data() + info.size()) -
              reinterpret_cast<const uint8_t *>(info.data()));
    }
  }
};

template <>
struct Codable<nf::RulesUpdate> {
  void Encode(Encoder &encoder, const nf::RulesUpdate &update) {
    encoder.EncodeInt32(int32_t(update.is_full));
    encoder.Encode(update.updated);
    encoder.EncodeInt32(static_cast<int32_t>(update.removed.size()));
    for (auto id : update.removed) {
      encoder.EncodeInt64(id);
    }
  }
};

}  // namespace mach

namespace {

nf::Application ResolveApplicationPath(const nf::Application &application) {
  if (auto bundle_path = ApplicationBundlePath(application.Path())) {
    return nf::Application{*bundle_path};
  } else {
    return application;
  }
}

void FixRule(nf::Rule &rule) {
  if (auto bundle_path = ApplicationBundlePath(rule.Application().Path())) {
    rule = nf::Rule(rule.Id(), rule.Permission(), nf::Application{*bundle_path},
                    rule.LastAccessTime(), rule.AccessCount());
  }
}

void FixRulesList(std::vector<nf::Rule> &rules) {
  for (auto &rule : rules) {
    FixRule(rule);
  }
}

class FilterDelegate {
 public:
  void RuleUpdated(const nf::Rule &rule) {
    client_port_.Use([&](auto &port) {
      if (port) {
        mach::Send(201, *port, rule);
      }
    });
  }

  template <class Completion>
  void HandlePackets(const nf::PacketList &packets, Completion &&completion) {
    client_port_.Use([&](auto &port) {
      if (!port) {
        return;
      }

      mach::Send(202, *port, packets, std::forward<Completion>(completion));
    });
  }

  nf::Time CurrentTime() const {
    return nf::Time::clock::from_time_t(std::time(nullptr));
  }

  template <class Completion>
  void AskPermission(const nf::Application &application,
                     Completion &&completion) {
    client_port_.Use([&](auto &port) {
      if (!port) {
        return;
      }

      mach::Send(203, *port, application, std::forward<Completion>(completion));
    });
  }

  void SetClientPort(std::optional<mach::SendRight> new_port) {
    client_port_.Use([&](auto &port) { port = std::move(new_port); });
  }

  template <class Completion>
  void SendRules(nf::RulesUpdate update, Completion &&completion) {
    client_port_.Use([&](auto &port) {
      if (!port) {
        return;
      }

      mach::Send(204, *port, update, std::forward<Completion>(completion));
    });
  }

 private:
  mcom::Sync<std::optional<mach::SendRight>> client_port_;
};

template <class Server, class Filter>
void SetupFilter(Server &server, Filter &filter) {
  // set mode
  server.AddHandler(201, [&](nf::FilterMode mode) { filter.SetMode(mode); });

  // get mode
  server.AddHandler(202, [&](mach::Promise<nf::FilterMode> result) {
    result(filter.GetMode());
  });

  // update rule
  server.AddHandler(204, [&](nf::Rule rule) {
    FixRule(rule);
    filter.UpdateRule(std::move(rule));
  });

  // remove rule
  server.AddHandler(205,
                    [&](nf::RuleId rule_id) { filter.RemoveRule(rule_id); });

  SetAccessCheckHandler([&](auto &application, auto &&completion) {
    return filter.CheckAccess(ResolveApplicationPath(application),
                              std::move(completion));
  });
}

std::optional<std::string> MachServiceName() {
  mcom::cf::Bundle main_bundle = mcom::cf::Bundle::GetMain();
  if (!main_bundle) {
    return std::nullopt;
  }

  mcom::cf::Dictionary ne_info =
      main_bundle.InfoDictionary().GetValue(CFSTR("NetworkExtension"));
  if (!ne_info) {
    return std::nullopt;
  }

  mcom::cf::String service_name = ne_info.GetValue(CFSTR("NEMachServiceName"));
  if (!service_name) {
    return std::nullopt;
  }

  return service_name.GetCString();
}

}  // namespace

int main(int argc, const char *argv[]) {
  std::optional<std::string> service_name = MachServiceName();
  if (!service_name) {
    os_log_error(OS_LOG_DEFAULT,
                 "failed to get service name from extension's Info.plist");
    std::exit(2);
  }

  EnableNetworkExtension();

  auto receive_right = mach::BootstrapCheckIn(service_name->c_str());
  if (!receive_right) {
    dispatch_main();
  }

  FilterDelegate delegate;
  nf::RulesStorage rules{[&](auto update, auto completion) {
    delegate.SendRules(std::move(update), std::move(completion));
  }};
  nf::PacketQueue packet_queue{[&](auto packet_list, auto completion) {
    delegate.HandlePackets(packet_list, std::move(completion));
  }};

  mach::Server server{*receive_right};

  // version check
  server.AddHandler(250, [](mach::Promise<> promise) { promise(); });

  // initialize filter
  server.AddHandler(
      251, [&](nf::FilterMode mode, std::vector<nf::Rule> rules_list,
               mach::Promise<> promise) {
        FixRulesList(rules_list);

        dispatch::Queue{}.Async(
            [&, mode, rules_list = std::move(rules_list), promise]() mutable {
              server.Suspend();

              static dispatch_once_t once;
              dispatch::Once(once, [&]() {
                static nf::NetworkFilter filter{mode, std::move(rules_list),
                                                delegate, &rules};

                SetupFilter(server, filter);
              });

              server.Resume();

              promise();
            });
      });

  // set delegate
  server.AddHandler(200, [&](mach::SendRight port) {
    delegate.SetClientPort(std::move(port));
    rules.ClientConnected();
  });

  // packet handler
  server.AddHandler(252, [](uint32_t flow_size, mach::SendRight port) {
    auto queue_handler = [port = std::move(port)](auto packets,
                                                  auto completion) mutable {
      auto error = mach::Send(202, port, packets, std::move(completion));
      if (error) {
        ResetPacketHandler();
      }
    };

    auto queue = std::make_shared<nf::PacketQueue<decltype(queue_handler)>>(
        std::move(queue_handler));

    SetPacketHandler(
        {flow_size, [queue](auto &packet) { queue->SendPacket(packet); }});
  });

  server.Resume();

  dispatch_main();
}
