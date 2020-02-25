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

#include "iokit.hpp"

#include <IOKit/IOBSD.h>
#include <mach/mach_error.h>

namespace {

class iokit_category_impl : public std::error_category {
  const char *name() const noexcept override { return "mcom::iokit"; }
  std::string message(int cnd) const override { return mach_error_string(cnd); }
};

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

}  // namespace

namespace mcom {

const std::error_category &iokit_category() noexcept {
  static const iokit_category_impl ecat;
  return ecat;
}

namespace iokit {

std::optional<Service> Iterator::Next() noexcept {
  if (auto svc = IOIteratorNext(it_)) {
    return std::optional<Service>{std::in_place, svc};
  }
  return {};
}

std::vector<Service> Iterator::AllValues() noexcept {
  std::vector<Service> services;
  while (auto svc = Next()) {
    services.emplace_back(std::move(*svc));
  }
  return services;
}

std::optional<Service> ServiceQuery::Iterator::Next() {
  if (auto io_it = std::get_if<iokit::Iterator>(&provider_)) {
    while (auto service = io_it->Next()) {
      if (!predicate_ || predicate_(*service)) {
        return service;
      }
    }
    return std::nullopt;
  } else {
    auto &child_it = *std::get_if<ServiceIterator>(&provider_);

    if (!child_it.io_iterator) {
      auto parent = child_it.provider->Next();
      if (!parent) {
        return std::nullopt;
      }

      child_it.io_iterator.emplace(((*parent.*child_it.getter)()).Value());

      return Next();
    }

    while (auto service = child_it.io_iterator->Next()) {
      if (!predicate_ || predicate_(*service)) {
        return service;
      }
    }

    if (auto parent = child_it.provider->Next()) {
      child_it.io_iterator.emplace(((*parent.*child_it.getter)()).Value());
      return Next();
    }

    return std::nullopt;
  }
}

ServiceQuery::ServiceQuery(class DiskName name)
    : query_{IOBSDNameMatching(kIOMasterPortDefault, 0,
                               name.ToString().c_str())} {}

ServiceQuery ServiceQuery::WithClassName(const char *name) {
  return ServiceQuery{IOServiceMatching(name), nullptr};
}

ServiceQuery ServiceQuery::ChildNamed(const char *name) const {
  Predicate predicate = [name = std::string{name}](auto &service) {
    return service.GetClass() == name;
  };

  return ServiceQuery{Children{std::make_shared<ServiceQuery>(*this)},
                      predicate};
}

ServiceQuery ServiceQuery::AnyChild() const {
  return ServiceQuery{Children{std::make_shared<ServiceQuery>(*this)}, nullptr};
}

ServiceQuery ServiceQuery::AnyParent() const {
  return ServiceQuery{Parents{std::make_shared<ServiceQuery>(*this)}, nullptr};
}

std::vector<Service> ServiceQuery::Find() const {
  std::vector<Service> services;
  Iterator iterator = ForEach();
  while (std::optional<Service> service = iterator.Next()) {
    services.emplace_back(std::move(*service));
  }
  return services;
}

ServiceQuery::Iterator ServiceQuery::ForEach() const {
  return Visit(
      query_,
      [&](cf::Dictionary match) {
        io_iterator_t iterator = 0;
        IOServiceGetMatchingServices(kIOMasterPortDefault,
                                     cf::RetainSafe(match.get()), &iterator);
        return Iterator{mcom::iokit::Iterator::Construct(iterator), predicate_};
      },
      [&](const Children &children) {
        return Iterator{
            std::make_unique<Iterator>(children.provider->ForEach()),
            &Service::ChildIterator, predicate_};
      },
      [&](const Parents &parents) {
        return Iterator{std::make_unique<Iterator>(parents.provider->ForEach()),
                        &Service::ParentIterator, predicate_};
      });
}

ServiceQuery ServiceQuery::Filter(Predicate predicate) const {
  if (predicate_) {
    return ServiceQuery{query_, [p1 = predicate_, p2 = predicate](auto &svc) {
                          return p1(svc) && p2(svc);
                        }};
  } else {
    return ServiceQuery{query_, predicate};
  }
}

PropertyQuery<DiskName> ServiceQuery::DiskName() const {
  return PropertyQuery<class DiskName>{*this, DiskNameForService};
}

std::optional<class DiskName> DiskNameForService(const Service &service) {
  auto value = service.template GetProperty<sizeof(io_name_t)>(kIOBSDNameKey);
  if (!value) {
    return std::nullopt;
  }
  return DiskName::FromString(*value);
}

}  // namespace iokit
}  // namespace mcom
