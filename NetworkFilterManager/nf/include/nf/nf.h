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

#include <os/base.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define NF_CLOSED_ENUM(_name, ...) \
  __enum_closed_decl(_name, unsigned, {__VA_ARGS__})

#define NF_OPTIONS(_name, ...) __options_closed_decl(_name, long, {__VA_ARGS__})

NF_CLOSED_ENUM(NF_FILTER_MODE, NF_FILTER_MODE_ALL_ALLOW OS_SWIFT_NAME(allAllow),
               NF_FILTER_MODE_ALL_DENY OS_SWIFT_NAME(allDeny),
               NF_FILTER_MODE_UNKNOWN_ALLOW OS_SWIFT_NAME(unknownAllow),
               NF_FILTER_MODE_UNKNOWN_DENY OS_SWIFT_NAME(unknownDeny),
               NF_FILTER_MODE_Wait OS_SWIFT_NAME(wait))
OS_SWIFT_NAME(FilterResult);

NF_CLOSED_ENUM(NF_RULE_PERMISSION,
               NF_RULE_PERMISSION_ALLOW OS_SWIFT_NAME(allow),
               NF_RULE_PERMISSION_DENY OS_SWIFT_NAME(deny))
OS_SWIFT_NAME(RulePermission);

NF_CLOSED_ENUM(NF_DIRECTION, NF_DIRECTION_INCOMING OS_SWIFT_NAME(incoming),
               NF_DIRECTION_OUTGOING OS_SWIFT_NAME(outgoing))
OS_SWIFT_NAME(PacketDirection);

NF_OPTIONS(NF_RULES_OPTIONS,
           NF_RULES_OPTIONS_SHOW_ALLOWED OS_SWIFT_NAME(showAllow) = 1,
           NF_RULES_OPTIONS_SHOW_DENIED OS_SWIFT_NAME(showDeny) = 2,
           NF_RULES_OPTIONS_SHOW_ALL OS_SWIFT_NAME(showAll) = 3, )
OS_SWIFT_NAME(RulesOptions);

NF_CLOSED_ENUM(NF_SORT_ORDER, NF_SORT_ORDER_NAME_ASC OS_SWIFT_NAME(appNameAZ),
               NF_SORT_ORDER_NAME_DESC OS_SWIFT_NAME(appNameZA),
               NF_SORT_ORDER_TIME_ASC OS_SWIFT_NAME(activeLast),
               NF_SORT_ORDER_TIME_DESC OS_SWIFT_NAME(activeFirst), )
OS_SWIFT_NAME(RulesSort);

OS_ASSUME_NONNULL_BEGIN

typedef time_t nf_time_t;

typedef struct {
  const char *path;
} nf_application_t;

typedef struct {
  uint64_t id;
  NF_RULE_PERMISSION permission;
  nf_application_t application;
  nf_time_t last_access;
  uint64_t access_count;
} nf_rule_t;

typedef struct {
  uint32_t size;
  NF_DIRECTION direction;
  nf_application_t application;
} nf_packet_t;

typedef struct {
  uint32_t size;
  NF_DIRECTION direction;
  nf_time_t time;
} nf_packet_info_t;

typedef struct {
  NF_RULES_OPTIONS mask;
  const char *_Nullable path;
} nf_rule_enumerator_options_t;

typedef struct {
  time_t date_from;
  time_t date_to;
  uint64_t bytes_incoming;
  uint64_t bytes_outgoing;
} nf_statistic_item_t;

typedef struct nf_manager *nf_manager_t;

typedef struct nf_rules_iterator *nf_rules_iterator_t;

typedef struct nf_rules_update *nf_rules_update_t;

typedef struct nf_statistics_store *nf_statistics_store_t;

typedef struct nf_app_statistics *nf_app_statistics_t;

__BEGIN_DECLS

nf_manager_t nf_manager_create(void);

void nf_manager_destroy(nf_manager_t manager);

nf_rules_iterator_t nf_manager_get_rules(nf_manager_t manager,
                                         nf_rule_enumerator_options_t options);

const nf_rule_t *_Nullable nf_rules_iterator_next(nf_rules_iterator_t iterator);

void nf_rules_iterator_destroy(nf_rules_iterator_t iterator);

nf_rules_update_t nf_rules_update_create(bool is_full);

void nf_rules_update_rule_updated(nf_rules_update_t update, nf_rule_t rule);

void nf_rules_update_rule_removed(nf_rules_update_t update, uint64_t rule_id);

void nf_rules_update_destroy(nf_rules_update_t update);

void nf_manager_rules_updated(nf_manager_t manager, nf_rules_update_t update);

nf_statistics_store_t nf_statistics_store_create(void);

void nf_statistics_store_destroy(nf_statistics_store_t store);

void nf_statistics_store_handle_packet_info(nf_statistics_store_t store,
                                            const char *application_path,
                                            nf_packet_info_t packet_info);

nf_app_statistics_t _Nullable nf_statistics_store_copy_app_statistics(
    nf_statistics_store_t store, const char *application_path);

nf_statistic_item_t *_Nullable nf_app_statistics_next(
    nf_app_statistics_t statistics);

void nf_app_statistics_destroy(nf_app_statistics_t statistics);

NF_RULE_PERMISSION nf_rule_permission_for_mode(NF_FILTER_MODE mode);

__END_DECLS

OS_ASSUME_NONNULL_END
