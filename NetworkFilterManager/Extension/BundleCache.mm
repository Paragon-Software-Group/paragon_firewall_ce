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
// Created by Alexey Antipov on 2019-12-16.
//

#import "BundleCache.hpp"

#import <unordered_map>

#import <Foundation/Foundation.h>

#import <mcom/sync.hpp>

namespace {

class BundleCache {
 public:
  std::optional<std::string> ApplicationBundlePath(const std::string &path) {
    if (auto bundle = GetBundle(path)) {
      return bundle.bundlePath.UTF8String;
    } else {
      return std::nullopt;
    }
  }

  NSBundle *GetBundle(const std::string &path) {
    return cache_.Use([&](auto &cache) {
      auto it = cache.find(path);
      if (it != cache.end()) {
        return it->second;
      }
      auto bundle = GetBundleDirect(path);
      cache[path] = bundle;
      return bundle;
    });
  }

  static NSBundle *GetBundleDirect(const std::string &path) {
    NSURL *url = [NSURL fileURLWithFileSystemRepresentation:path.c_str()
                                                isDirectory:NO
                                              relativeToURL:nil];
    const auto path_components = url.pathComponents;

    if (path_components.count == 0) {
      return nil;
    }

    for (NSUInteger index = 1; index < path_components.count; ++index) {
      NSArray *bundle_path_components = [path_components subarrayWithRange:NSMakeRange(0, index)];
      NSString *bundle_path = [NSString pathWithComponents:bundle_path_components];
      NSURL *bundle_url = [NSURL fileURLWithPath:bundle_path];

      NSDictionary *resource_values = [bundle_url resourceValuesForKeys:@[ NSURLTypeIdentifierKey ]
                                                                  error:nil];
      NSString *type = resource_values[NSURLTypeIdentifierKey];
      if (nil == type) {
        continue;
      }
      if (!UTTypeConformsTo((__bridge CFStringRef)type, kUTTypeBundle)) {
        continue;
      }

      if (auto bundle = [NSBundle bundleWithURL:bundle_url]) {
        return bundle;
      }
    }
    return nil;
  }

 private:
  using Cache = std::unordered_map<std::string, NSBundle *>;
  mcom::Sync<Cache> cache_;
};

}  // namespace

std::optional<std::string> ApplicationBundlePath(const std::string &path) {
  static BundleCache bundle_cache_;
  return bundle_cache_.ApplicationBundlePath(path);
}
