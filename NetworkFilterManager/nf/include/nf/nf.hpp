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

#pragma once

#include <nf/nf.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <mcom/dispatch.hpp>
#include <mcom/sync.hpp>

namespace nf {

enum class FilterMode { AllAllow, AllDeny, UnknownAllow, UnknownDeny, Wait };

enum class AccessStatus { Allow, Deny, Wait };

enum class RulePermission { Allow, Deny };

constexpr AccessStatus ToAccessStatus(RulePermission permission) {
  switch (permission) {
    case RulePermission::Allow:
      return AccessStatus::Allow;
    case RulePermission::Deny:
      return AccessStatus::Deny;
  }
}

constexpr RulePermission RulePermissionForMode(FilterMode mode) {
  switch (mode) {
    case FilterMode::AllAllow:
    case FilterMode::UnknownAllow:
      return RulePermission::Allow;

    case FilterMode::AllDeny:
    case FilterMode::UnknownDeny:
    default:
      return RulePermission::Deny;
  }
}

class Application {
 public:
  Application(std::string_view path)
      : path_{std::make_shared<const std::string>(path)} {}

  const std::string &Path() const { return *path_; }

  Application(const Application &) = default;

  Application &operator=(const Application &other) {
    path_ = other.path_;
    return *this;
  }

  Application(Application &&other) = delete;

  Application &operator=(Application &&) = delete;

 private:
  std::shared_ptr<const std::string> path_;
};

}  // namespace nf

namespace std {

template <>
struct hash<nf::Application> {
  size_t operator()(const nf::Application &app) const noexcept {
    return std::hash<std::string>{}(app.Path());
  }
};

}  // namespace std

namespace nf {

inline bool operator==(const Application &lhs, const Application &rhs) {
  return lhs.Path() == rhs.Path();
}

using Time = std::chrono::system_clock::time_point;

using RuleId = uint64_t;

class Rule {
 public:
  Rule(RuleId id, RulePermission permission, const Application &application)
      : id_{id}, permission_{permission}, application_{application} {}

  Rule(RuleId id, RulePermission permission, const Application &application,
       const std::optional<Time> &last_access, uint64_t access_count)
      : id_{id},
        permission_{permission},
        application_{application},
        last_access_{last_access},
        access_count_{access_count} {}

  RuleId Id() const noexcept { return id_; }

  RulePermission Permission() const noexcept { return permission_; }

  const Application &Application() const { return application_; }

  const std::optional<Time> LastAccessTime() const { return last_access_; }

  uint64_t AccessCount() const { return access_count_; }

  Rule WithId(RuleId id) const {
    return {id, Permission(), Application(), LastAccessTime(), AccessCount()};
  }

  Rule WithAccessTime(const Time &time) const {
    return {Id(), Permission(), Application(), time, AccessCount()};
  }

  Rule WithPermission(RulePermission permission) const {
    return {Id(), permission, Application(), LastAccessTime(), AccessCount()};
  }

 private:
  RuleId id_;
  RulePermission permission_;
  class Application application_;
  std::optional<Time> last_access_ = std::nullopt;
  uint64_t access_count_ = 0;
};

class Packet {
 public:
  using TimeType = Time;

  enum class Direction { Incoming, Outgoing };

  Packet(uint32_t size, Direction direction, const Application &application)
      : size_{size}, direction_{direction}, application_{application} {}

  uint32_t Size() const { return size_; }

  Direction PacketDirection() const { return direction_; }

  const Application &Application() const { return application_; }

  const TimeType &Time() const { return time_; }

 private:
  uint32_t size_;
  Direction direction_;
  class Application application_;
  TimeType time_;
};

class Deferred {
 public:
  using Handler = std::function<void()>;

  template <class Fn>
  Deferred(Fn &&fn) : handler_{std::forward<Fn>(fn)} {}

  template <class Fn>
  static std::shared_ptr<Deferred> Shared(Fn &&fn) {
    return std::make_shared<Deferred>(std::forward<Fn>(fn));
  }

  Deferred &operator=(Deferred &&) = delete;

  ~Deferred() {
    if (handler_) {
      handler_();
    }
  }

  void Cancel() { handler_ = nullptr; }

 private:
  Handler handler_;
};

struct RulesUpdate {
  bool is_full;
  std::vector<Rule> updated;
  std::vector<RuleId> removed;
};

using AccessCheckCompletion = std::function<void(AccessStatus)>;
using AccessCheckHandler = std::function<AccessStatus(
    const nf::Application &application, AccessCheckCompletion)>;

template <class Callback>
class RulesStorage {
 public:
  RulesStorage(Callback callback)
      : callback_{std::forward<Callback>(callback)} {}

  RulesStorage &operator=(RulesStorage &&) = delete;

  void UpdateRule(Rule rule) {
    auto guard = lock_.Lock();

    if (rule.Id() == 0) {
      // check if a rule for the same application exists
      auto it = std::find_if(rules_.begin(), rules_.end(), [&](auto &kv) {
        return rule.Application().Path() == kv.second.Application().Path();
      });
      if (it == rules_.end()) {
        rule = rule.WithId(last_id_++);
      } else {
        const auto &found_rule = it->second;
        rule = found_rule.WithPermission(rule.Permission());
      }
    }

    // update rules list
    auto emplace_result = rules_.emplace(rule.Id(), rule);
    if (!emplace_result.second) {
      emplace_result.first->second = rule;
    }

    if (!client_connected_) {
      return;
    }

    if (in_progress_) {
      pending_update_.updated.insert(rule.Id());
    } else {
      in_progress_ = true;
      SendUpdate(CollectChanges({{rule.Id()}, {}}));
    }
  }

  void RemoveRule(RuleId rule_id) {
    auto guard = lock_.Lock();

    // update rules list
    if (0 == rules_.erase(rule_id)) {
      return;
    }

    if (!client_connected_) {
      return;
    }

    if (in_progress_) {
      pending_update_.updated.erase(rule_id);
      pending_update_.removed.insert(rule_id);
    } else {
      in_progress_ = true;
      SendUpdate(CollectChanges({{}, {rule_id}}));
    }
  }

  template <class Fn>
  void ModifyInPlace(RuleId rule_id, Fn &&fn) {
    auto guard = lock_.Lock();

    auto it = rules_.find(rule_id);
    if (it == rules_.end()) {
      return;
    }

    fn(it->second);

    if (!client_connected_) {
      return;
    }

    if (in_progress_) {
      pending_update_.updated.insert(rule_id);
    } else {
      in_progress_ = true;
      SendUpdate(CollectChanges({{rule_id}, {}}));
    }
  }

  template <class Predicate>
  std::optional<Rule> Matching(Predicate &&predicate) const {
    auto guard = lock_.Lock();

    auto it = std::find_if(rules_.begin(), rules_.end(),
                           [&](auto &kv) { return predicate(kv.second); });
    if (it == rules_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  void ClientConnected() {
    auto guard = lock_.Lock();

    if (in_progress_) {
      client_reconnected_ = true;
      return;
    }

    client_connected_ = true;
    in_progress_ = true;

    SendUpdate(FullUpdate());
  }

 private:
  struct Update {
    std::unordered_set<RuleId> updated;
    std::unordered_set<RuleId> removed;

    void Clear() {
      updated.clear();
      removed.clear();
    }

    bool IsEmpty() const { return updated.empty() && removed.empty(); }
  };

  void SendUpdate(RulesUpdate changes) {
    auto no_reply = Deferred::Shared([this]() {
      dispatch::Queue{}.Async([this]() { DidSendUpdate(false); });
    });

    SendUpdate(std::move(changes), [this, no_reply]() {
      no_reply->Cancel();
      DidSendUpdate(true);
    });
  }

  template <class Fn>
  void SendUpdate(RulesUpdate changes, Fn &&fn) {
    dispatch::Queue{}.Async([this, changes = std::move(changes),
                             fn = std::forward<Fn>(fn)]() mutable {
      callback_(std::move(changes), std::forward<Fn>(fn));
    });
  }

  void DidSendUpdate(bool success) {
    auto guard = lock_.Lock();

    if (client_reconnected_) {
      // client reconnected while storage were in progress
      client_connected_ = true;
      client_reconnected_ = false;
      pending_update_.Clear();
      SendUpdate(FullUpdate());
      return;
    }

    if (!success) {
      // client disconnected. erase all the cache
      client_connected_ = false;
      pending_update_.Clear();
      in_progress_ = false;
      return;
    }

    // successfully sent an update to a client

    if (pending_update_.IsEmpty()) {
      in_progress_ = false;
      return;
    }

    SendUpdate(CollectChanges(pending_update_));
    pending_update_.Clear();
  }

  RulesUpdate CollectChanges(const Update &update) {
    RulesUpdate changes;
    changes.is_full = false;
    for (auto id : update.updated) {
      changes.updated.push_back(rules_.at(id));
    }
    for (auto id : update.removed) {
      changes.removed.push_back(id);
    }
    return changes;
  }

  RulesUpdate FullUpdate() {
    RulesUpdate changes;
    changes.is_full = true;
    for (auto &kv : rules_) {
      changes.updated.push_back(kv.second);
    }
    return changes;
  }

  mutable dispatch::Semaphore lock_{1};
  std::atomic<RuleId> last_id_{1};
  std::unordered_map<RuleId, Rule> rules_;
  bool client_connected_ = false;
  bool client_reconnected_ = false;
  bool in_progress_ = false;
  Update pending_update_;
  Callback callback_;
};

class PacketList {
 public:
  using StorageType =
      std::unordered_map<Application, std::vector<nf_packet_info_t>>;

  void Add(const Packet &packet) {
    auto insert_result = packets_.insert({packet.Application(), {}});
    insert_result.first->second.push_back(
        {packet.Size(), NF_DIRECTION(packet.PacketDirection()),
         Time::clock::to_time_t(packet.Time())});
  }

  void Clear() { packets_.clear(); }

  bool IsEmpty() const { return packets_.empty(); }

  const StorageType &Storage() const { return packets_; }

 private:
  StorageType packets_;
};

template <class Handler>
class PacketQueue {
 public:
  PacketQueue(Handler &&handler) : handler_{std::forward<Handler>(handler)} {
    timer_.SetEventHandler([this]() { HandleTimer(); });
    timer_.Schedule(dispatch::Time::Now(), dispatch::Duration::Seconds(1));
    timer_.Resume();
  }

  PacketQueue &operator=(PacketQueue &&) = delete;

  void SendPacket(Packet packet) {
    if (packet.Size() <= 0) {
      return;
    }

    queue_.Async([this, packet]() { EnqueuePacket(packet); });
  }

 private:
  void EnqueuePacket(Packet packet) { list_.Add(packet); }

  void DidSendPacketList() { in_progress_ = false; }

  void HandleTimer() {
    if (in_progress_ || list_.IsEmpty()) {
      return;
    }

    in_progress_ = true;

    dispatch::Queue{}.Async([this, list = std::move(list_)]() mutable {
      auto did_finish = Deferred::Shared([this]() { DidSendPacketList(); });

      handler_(std::move(list), [did_finish = std::move(did_finish)]() {});
    });

    list_.Clear();
  }

  Handler handler_;
  dispatch::Queue queue_{"com.paragon-software.FirewallApp.PacketQueue"};
  dispatch::Timer timer_{queue_};
  bool in_progress_ = false;
  PacketList list_;
};

template <class Delegate, class RulesStorage>
class NetworkFilter {
 public:
  using RulesList = std::vector<Rule>;

  NetworkFilter(FilterMode mode, RulesList &&initial_rules, Delegate &delegate,
                RulesStorage &&rules)
      : mode_{mode},
        delegate_{delegate},
        rules_{std::forward<RulesStorage>(rules)} {
    for (auto &rule : initial_rules) {
      rules_->UpdateRule(std::move(rule));
    }
  }

  NetworkFilter &operator=(NetworkFilter &&) = delete;

  void SetMode(FilterMode mode) noexcept { mode_ = mode; }

  FilterMode GetMode() const noexcept { return mode_; }

  void UpdateRule(Rule rule) noexcept { rules_->UpdateRule(rule); }

  void RemoveRule(uint64_t rule_id) noexcept { rules_->RemoveRule(rule_id); }

  template <class Completion>
  AccessStatus CheckAccess(const Application &application,
                           Completion &&completion) noexcept {
    if (auto rule = RuleMatchingApplication(application)) {
      if (mode_ == FilterMode::AllAllow) {
        return AccessStatus::Allow;
      }

      const auto access_status = ToAccessStatus(rule->Permission());
      if (access_status == AccessStatus::Allow) {
        UpdateRuleAccessTime(rule->Id());
      }
      return access_status;
    }

    if (mode_ == FilterMode::Wait) {
      auto &path = application.Path();

      const auto should_ask = completions_.Use([&](auto &completions) -> bool {
        auto insert_result = completions.insert({path, {}});

        insert_result.first->second.emplace_back(
            std::forward<Completion>(completion));

        return insert_result.second;
      });

      if (should_ask) {
        auto lambda = [this, path](AccessStatus permission) {
          completions_.Use([&](auto &completions) {
            for (auto &completion : completions[path]) {
              completion(permission);
            }
            completions.erase(path);
          });
        };

        auto shared_completion =
            std::make_shared<decltype(lambda)>(std::move(lambda));

        auto caller = Deferred::Shared([shared_completion]() {
          (*shared_completion)(AccessStatus::Allow);
        });

        delegate_.AskPermission(
            application, [=](nf::RulePermission permission) mutable {
              caller->Cancel();
              const auto access_status =
                  AccessStatusWithNewRule(permission, application);
              (*shared_completion)(access_status);
            });
      }

      return AccessStatus::Wait;
    }

    return AccessStatusWithNewRule(RulePermissionForMode(mode_), application);
  }

  std::optional<Rule> RuleMatchingApplication(
      const Application &application) const noexcept {
    return rules_->Matching([&](auto &rule) {
      return rule.Application().Path() == application.Path();
    });
  }

  Time CurrentTime() const { return delegate_.CurrentTime(); }

 private:
  void UpdateRuleAccessTime(RuleId rule_id) {
    rules_->ModifyInPlace(rule_id, [time = CurrentTime()](auto &rule) {
      rule = rule.WithAccessTime(time);
    });
  }

  AccessStatus AccessStatusWithNewRule(RulePermission permission,
                                       const Application &application) {
    auto new_rule = (permission == RulePermission::Allow)
                        ? CreateRuleWithAccessTime(permission, application)
                        : CreateRule(permission, application);
    UpdateRule(new_rule);
    return ToAccessStatus(permission);
  }

  Rule CreateRule(RulePermission permission,
                  const Application &application) const {
    return Rule{0, permission, application};
  }

  Rule CreateRuleWithAccessTime(RulePermission permission,
                                const Application &application) const {
    return Rule{0, permission, application, CurrentTime(), 1};
  }

  std::atomic<FilterMode> mode_;
  Delegate &delegate_;
  RulesStorage rules_;

  mcom::Sync<
      std::unordered_map<std::string, std::vector<AccessCheckCompletion>>>
      completions_;
};

constexpr std::optional<Time> FromTimeT(std::time_t value) {
  return value != 0 ? std::optional{Time::clock::from_time_t(value)}
                    : std::nullopt;
}

constexpr time_t ToTimeT(std::optional<Time> time) {
  return time ? Time::clock::to_time_t(*time) : 0;
}

constexpr RulePermission Convert(NF_RULE_PERMISSION permission) {
  switch (permission) {
    case NF_RULE_PERMISSION_ALLOW:
      return RulePermission::Allow;
    case NF_RULE_PERMISSION_DENY:
      return RulePermission::Deny;
  }
}

constexpr NF_RULE_PERMISSION Convert(RulePermission permission) {
  switch (permission) {
    case RulePermission::Allow:
      return NF_RULE_PERMISSION_ALLOW;
    case RulePermission::Deny:
      return NF_RULE_PERMISSION_DENY;
  }
}

constexpr Packet::Direction Convert(NF_DIRECTION direction) {
  switch (direction) {
    case NF_DIRECTION_INCOMING:
      return Packet::Direction::Incoming;
    case NF_DIRECTION_OUTGOING:
      return Packet::Direction::Outgoing;
  }
}

}  // namespace nf
