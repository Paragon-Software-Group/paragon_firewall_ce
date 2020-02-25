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

#pragma once

#include <IOKit/IOKitLib.h>
#include <optional>
#include <string>
#include <system_error>
#include <variant>
#include <vector>

#include <mcom/cf.hpp>
#include <mcom/disk_name.hpp>
#include <mcom/result.hpp>

namespace mcom {

const std::error_category &iokit_category() noexcept;

namespace iokit {

class Service;

class Iterator {
 public:
  Iterator(Iterator &&other) noexcept : it_{other.it_} {
    other.it_ = IO_OBJECT_NULL;
  }

  Iterator(const Iterator &) = delete;

  ~Iterator() {
    if (it_) {
      IOObjectRelease(it_);
    }
  }

  std::optional<Service> Next() noexcept;

  std::vector<Service> AllValues() noexcept;

  static Iterator Construct(io_iterator_t it) noexcept { return Iterator{it}; }

 private:
  friend class Service;

  Iterator(io_iterator_t it) : it_(it) {}

  io_iterator_t it_;
};

class Service {
 public:
  explicit Service(io_service_t svc) noexcept : service_(svc) {
    if (!svc) {
      abort();
    }
  }

  Service(Service &&other) noexcept : service_(other.service_) {
    other.service_ = IO_OBJECT_NULL;
  }

  explicit Service(const Service &);

  ~Service() {
    if (service_) {
      IOObjectRelease(service_);
    }
  }

  template <size_t Size>
  Result<std::string> GetProperty(const char *name, bool raw = false) const
      noexcept {
    char buffer[Size];
    uint32_t size = Size;
    if (auto status =
            IORegistryEntryGetProperty(service_, name, buffer, &size)) {
      return std::error_code{status, iokit_category()};
    }
    if (!raw) {
      if (auto end_ptr = std::memchr(buffer, '\0', size)) {
        size = static_cast<uint32_t>(static_cast<char *>(end_ptr) - buffer);
      }
    }
    return Result<std::string>{in_place, buffer, size};
  }

  Result<std::string> GetClass() const noexcept {
    io_name_t class_name;
    if (auto status = IOObjectGetClass(service_, class_name)) {
      return std::error_code{status, iokit_category()};
    }
    return Result<std::string>{class_name};
  }

  cf::TypePtr createCFProperty(CFStringRef key) const noexcept {
    return cf::TypePtr::FromRetained(
        IORegistryEntryCreateCFProperty(service_, key, kCFAllocatorDefault, 0));
  }

  Result<Service> Parent() noexcept {
    io_registry_entry_t parent;
    if (auto status =
            IORegistryEntryGetParentEntry(service_, kIOServicePlane, &parent)) {
      return std::error_code{status, iokit_category()};
    }
    return Service{parent};
  }

  Result<Iterator> ParentIterator() noexcept {
    io_iterator_t it;
    if (auto status =
            IORegistryEntryGetParentIterator(service_, kIOServicePlane, &it)) {
      return std::error_code{status, iokit_category()};
    }
    return Iterator{it};
  }

  Result<Service> Child() noexcept {
    io_registry_entry_t child;
    if (auto status =
            IORegistryEntryGetChildEntry(service_, kIOServicePlane, &child)) {
      return std::error_code{status, iokit_category()};
    }
    return Service{child};
  }

  Result<Iterator> ChildIterator() noexcept {
    io_iterator_t it;
    if (auto status =
            IORegistryEntryGetChildIterator(service_, kIOServicePlane, &it)) {
      return std::error_code{status, iokit_category()};
    }
    return Iterator{it};
  }

  static Optional<Service> MatchingService(CFDictionaryRef matching) noexcept {
    if (auto service =
            IOServiceGetMatchingService(kIOMasterPortDefault, matching)) {
      return Service{service};
    }
    return {};
  }

  static Result<Iterator> MatchingServices(CFDictionaryRef matching) noexcept {
    io_iterator_t it;
    if (auto status =
            IOServiceGetMatchingServices(kIOMasterPortDefault, matching, &it)) {
      return std::error_code{status, iokit_category()};
    }
    return Iterator{it};
  }

  static Optional<Service> MatchingBSDName(const char *bsd_name) noexcept {
    return MatchingService(
        IOBSDNameMatching(kIOMasterPortDefault, 0, bsd_name));
  }

  static Optional<Service> MatchingName(const char *name) noexcept {
    return MatchingService(IOServiceNameMatching(name));
  }

  io_service_t Handle() noexcept { return service_; }

 private:
  io_service_t service_;
};

template <class T>
class PropertyQuery;

template <class Getter>
struct PropQueryTypeHelper;

template <class Getter>
using PropQueryType = typename PropQueryTypeHelper<Getter>::type;

class ServiceQuery {
 public:
  using Predicate = std::function<bool(const Service &service)>;

  explicit ServiceQuery(DiskName name);

  static ServiceQuery WithClassName(const char *name);

  ServiceQuery ChildNamed(const char *name) const;

  ServiceQuery AnyChild() const;

  ServiceQuery AnyParent() const;

  ServiceQuery Filter(Predicate predicate) const;

  PropertyQuery<DiskName> DiskName() const;

  template <class Getter>
  PropQueryType<Getter> Property(Getter &&getter);

  std::optional<Service> FindFirst() const;

  std::vector<Service> Find() const;

  class Iterator {
   public:
    std::optional<Service> Next();

   private:
    friend class ServiceQuery;

    using Predicate = std::function<bool(const Service &service)>;

    using IteratorGetter = Result<iokit::Iterator> (Service::*)();

    struct ServiceIterator {
      std::unique_ptr<Iterator> provider;
      IteratorGetter getter;
      std::optional<iokit::Iterator> io_iterator;
    };

    using Provider = std::variant<iokit::Iterator, ServiceIterator>;

    Iterator(iokit::Iterator iterator, Predicate predicate)
        : provider_{std::move(iterator)}, predicate_{predicate} {}

    Iterator(std::unique_ptr<Iterator> provider, IteratorGetter getter,
             Predicate predicate)
        : provider_{ServiceIterator{std::move(provider), getter, std::nullopt}},
          predicate_{predicate} {}

    Provider provider_;
    Predicate predicate_;
  };

  Iterator ForEach() const;

 private:
  struct Children {
    std::shared_ptr<ServiceQuery> provider;
  };

  struct Parents {
    std::shared_ptr<ServiceQuery> provider;
  };

  using Query = std::variant<cf::Dictionary, Children, Parents>;

  explicit ServiceQuery(Query query, Predicate predicate)
      : query_{query}, predicate_{predicate} {}

  Query query_;
  Predicate predicate_;
};

template <class T>
class PropertyQuery {
 public:
  std::optional<T> First() const { return ForEach().Next(); }

  std::vector<T> Find() const {
    std::vector<T> result;
    Iterator it = ForEach();
    while (auto value = it.Next()) {
      result.emplace_back(std::move(*value));
    }
    return result;
  }

  class Iterator {
   public:
    std::optional<T> Next() { return provider_(); }

   private:
    friend class PropertyQuery;

    using Provider = std::function<std::optional<T>()>;

    Iterator(Provider provider) : provider_{std::move(provider)} {}

    Provider provider_;
  };

  Iterator ForEach() const {
    auto it = std::make_shared<ServiceQuery::Iterator>(service_.ForEach());

    return Iterator([it, get_prop = getter_]() -> std::optional<T> {
      while (auto service = it->Next()) {
        if (auto value = get_prop(*service)) {
          return value;
        }
      }
      return std::nullopt;
    });
  }

 private:
  friend class ServiceQuery;

  using Getter = std::function<std::optional<T>(const Service &)>;

  explicit PropertyQuery(ServiceQuery service, Getter getter)
      : service_{service}, getter_{getter} {}

  ServiceQuery service_;
  Getter getter_;
};

template <class Getter>
PropQueryType<Getter> ServiceQuery::Property(Getter &&getter) {
  return PropQueryType<Getter>{*this, getter};
}

template <class Getter>
struct PropQueryTypeHelper;

template <class Getter>
struct PropQueryTypeHelper {
  using RetType =
      decltype(std::declval<const Getter>()(std::declval<const Service &>()));
  using ValueType = std::decay_t<typename RetType::value_type>;

  using type = PropertyQuery<ValueType>;
};

std::optional<class DiskName> DiskNameForService(const Service &service);

}  // namespace iokit
}  // namespace mcom
