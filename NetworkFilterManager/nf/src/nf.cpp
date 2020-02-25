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

#include <nf/nf.hpp>

#include <array>
#include <ctime>
#include <optional>

struct nf_rules_update {
  bool is_full;
  std::vector<nf::Rule> rules;
  std::vector<uint64_t> removed;
};

struct nf_app_statistics {
  using ItemsType = std::vector<nf_statistic_item_t>;

  ItemsType items;
  std::optional<ItemsType::iterator> iterator;
};

static inline constexpr size_t kMaxHours = 24;

class nf_manager {
 public:
  void RulesUpdated(nf_rules_update update) {
    rules_.Use([&](auto &rules) {
      if (update.is_full) {
        rules.clear();
        for (auto &rule : update.rules) {
          rules.emplace(rule.Id(), rule);
        }
      } else {
        for (auto &rule : update.rules) {
          rules.insert_or_assign(rule.Id(), rule);
        }
        for (auto id : update.removed) {
          rules.erase(id);
        }
      }
    });
  }

  std::vector<nf::Rule> Rules(
      const nf_rule_enumerator_options_t &options) const {
    std::vector<nf::Rule> rules;

    rules_.Use([&](auto &rules_) {
      for (const auto &kv : rules_) {
        auto &rule = kv.second;

        if ((rule.Permission() == nf::RulePermission::Allow &&
             (options.mask & NF_RULES_OPTIONS_SHOW_ALLOWED)) ||
            (rule.Permission() == nf::RulePermission::Deny &&
             (options.mask & NF_RULES_OPTIONS_SHOW_DENIED))) {
          rules.push_back(rule);
        }
      }
    });

    return rules;
  }

 private:
  mcom::Sync<std::unordered_map<nf::RuleId, nf::Rule>> rules_;
};

class nf_statistics_store {
 public:
  void HandlePacket(const char *application_path,
                    const nf_packet_info_t &info) {
    if (info.size <= 0) {
      return;
    }

    auto guard = statistic_lock_.Lock();

    const auto now = nf::Time::clock::now();
    const auto max_interval = std::chrono::hours{kMaxHours};
    const auto validity_start = now - max_interval;

    if (now.time_since_epoch() < max_interval) {
      return;
    }

    auto emplace_result = statistic_.emplace(
        application_path,
        StatisticData{nf::Time::clock::from_time_t(info.time), {}});
    auto &data = emplace_result.first->second;

    auto &traffic = [&]() -> StatisticData::Traffic & {
      // TODO: check for overflows

      auto &traffic_by_hour = data.traffic_by_hour;

      auto first_valid_hour = std::find_if(
          traffic_by_hour.begin(), traffic_by_hour.end(), [&](auto &it) {
            return data.from +
                       std::chrono::hours{&it - traffic_by_hour.begin()} >=
                   validity_start;
          });

      if (first_valid_hour == traffic_by_hour.end()) {
        // outdated/invalid statistic, clear it
        data.from = now;
        traffic_by_hour.fill({0, 0});
        return traffic_by_hour[0];
      } else if (first_valid_hour != traffic_by_hour.begin()) {
        // drop outdated statistics
        auto outdated_hours = first_valid_hour - traffic_by_hour.begin();
        std::move(traffic_by_hour.begin() + outdated_hours,
                  traffic_by_hour.end(), traffic_by_hour.begin());
        data.from += std::chrono::hours{outdated_hours};
        std::fill(traffic_by_hour.end() - outdated_hours, traffic_by_hour.end(),
                  StatisticData::Traffic{0, 0});
        first_valid_hour = traffic_by_hour.begin();
      }

      return *first_valid_hour;
    }();

    switch (info.direction) {
      case NF_DIRECTION_INCOMING:
        traffic.incoming += info.size;
        break;

      case NF_DIRECTION_OUTGOING:
        traffic.outgoing += info.size;
        break;

      default:
        break;
    }
  }

  nf_app_statistics_t CopyStatistic(std::string_view application_path) {
    auto guard = statistic_lock_.Lock();

    auto it = statistic_.find(std::string{application_path});
    if (it == statistic_.end()) {
      return nullptr;
    }

    auto &data = it->second;
    auto list = new nf_app_statistics;

    for (auto hour = 0; hour < kMaxHours; ++hour) {
      auto &traffic = data.traffic_by_hour[hour];
      if (traffic.incoming + traffic.outgoing == 0) {
        continue;
      }

      list->items.push_back(
          {nf::Time::clock::to_time_t(data.from + std::chrono::hours{hour}),
           nf::Time::clock::to_time_t(data.from + std::chrono::hours{hour + 1}),
           traffic.incoming, traffic.outgoing});
    }

    return list;
  }

 private:
  struct StatisticData {
    struct Traffic {
      uint64_t incoming;
      uint64_t outgoing;
    };
    nf::Time from;
    std::array<Traffic, kMaxHours> traffic_by_hour;
  };

  std::unordered_map<std::string, StatisticData> statistic_;
  dispatch::Semaphore statistic_lock_{1};
};

class nf_rules_iterator {
 public:
  using Storage = std::vector<nf::Rule>;
  using Iterator = Storage::const_iterator;

  nf_rules_iterator(Storage rules) : rules_{std::move(rules)} {}

  const nf_rule_t *_Nullable Next() {
    if (!iterator_) {
      iterator_ = rules_.cbegin();
    }
    auto &iterator = *iterator_;

    if (iterator == rules_.end()) {
      return nullptr;
    }

    auto &rule = *(iterator++);

    rule_buffer_.id = rule.Id();
    rule_buffer_.permission = Convert(rule.Permission());
    rule_buffer_.application.path = rule.Application().Path().c_str();
    rule_buffer_.last_access = nf::ToTimeT(rule.LastAccessTime());
    rule_buffer_.access_count = rule.AccessCount();

    return &rule_buffer_;
  }

 private:
  const Storage rules_;
  std::optional<Iterator> iterator_;
  nf_rule_t rule_buffer_;
};

nf_manager_t nf_manager_create(void) { return new nf_manager; }

void nf_manager_destroy(nf_manager_t manager) { delete manager; }

nf_rules_iterator_t nf_manager_get_rules(nf_manager_t manager,
                                         nf_rule_enumerator_options_t options) {
  return new nf_rules_iterator{manager->Rules(options)};
}

const nf_rule_t *_Nullable nf_rules_iterator_next(
    nf_rules_iterator_t iterator) {
  return iterator->Next();
}

void nf_rules_iterator_destroy(nf_rules_iterator_t iterator) {
  delete iterator;
}

nf_rules_update_t nf_rules_update_create(bool is_full) {
  return new nf_rules_update{is_full};
}

void nf_rules_update_rule_updated(nf_rules_update_t update, nf_rule_t rule) {
  update->rules.emplace_back(rule.id, nf::Convert(rule.permission),
                             nf::Application{rule.application.path},
                             nf::FromTimeT(rule.last_access),
                             rule.access_count);
}

void nf_rules_update_rule_removed(nf_rules_update_t update, uint64_t rule_id) {
  update->removed.push_back(rule_id);
}

void nf_rules_update_destroy(nf_rules_update_t update) { delete update; }

void nf_manager_rules_updated(nf_manager_t manager, nf_rules_update_t update) {
  manager->RulesUpdated(std::move(*update));
}

nf_statistic_item_t *_Nullable nf_app_statistics_next(
    nf_app_statistics_t statistics) {
  if (!statistics->iterator) {
    statistics->iterator = statistics->items.begin();
  }
  return (statistics->iterator != statistics->items.end())
             ? &(*statistics->iterator.value()++)
             : nullptr;
}

void nf_app_statistics_destroy(nf_app_statistics_t statistics) {
  delete statistics;
}

nf_statistics_store_t nf_statistics_store_create(void) {
  return new nf_statistics_store;
}

void nf_statistics_store_destroy(nf_statistics_store_t store) { delete store; }

void nf_statistics_store_handle_packet_info(nf_statistics_store_t store,
                                            const char *application_path,
                                            nf_packet_info_t packet_info) {
  store->HandlePacket(application_path, packet_info);
}

nf_app_statistics_t _Nullable nf_statistics_store_copy_app_statistics(
    nf_statistics_store_t store, const char *application_path) {
  return store->CopyStatistic(application_path);
}

NF_RULE_PERMISSION nf_rule_permission_for_mode(NF_FILTER_MODE mode) {
  return static_cast<NF_RULE_PERMISSION>(
      RulePermissionForMode(static_cast<nf::FilterMode>(mode)));
}
