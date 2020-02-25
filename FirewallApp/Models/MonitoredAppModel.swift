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
// Created by kaz on 22.07.2019.
//

import Foundation
import Combine
import SwiftUI
import os.log
import NetworkFilter
import nf

struct ApplicationInfo: Hashable {
  var name: String
  var icon: NSImage
  var displayPath: String
}

struct MonitoredAppModel: Identifiable, Hashable {
  var rule: Rule
  var id: Rule.ID { rule.id }
  
  var applicationInfo: ApplicationInfo
  var name: String { applicationInfo.name }
  var icon: NSImage { applicationInfo.icon }
  var permission: RulePermission { rule.permission }
  
  var statistic: [Statistic]?
  
  var traffic: Int64? {
    statistic.map { statistic in
      Int64(statistic.reduce(0) { result, statistic in
        result + statistic.bytesIncoming + statistic.bytesOutgoing
      })
    }
  }
  
  var lastAccess: Date? {
    return self.rule.lastAccess
  }
  
  init(rule: Rule, statistic: [Statistic]?) {
    self.rule = rule
    self.applicationInfo = getApplicationInfo(rule.application.path)
    self.statistic = statistic
  }
}

func getApplicationInfo(_ path: String) -> ApplicationInfo {
  let bundle = getBundle(containing: URL(fileURLWithPath: path))
  let displayPath = bundle.flatMap { $0.executablePath == path ? $0.bundlePath : nil } ?? path
  
  return ApplicationInfo(
    name: FileManager.default.displayName(atPath: displayPath),
    icon: NSWorkspace.shared.icon(forFile: displayPath),
    displayPath: displayPath
  )
}

func getBundle(containing path: URL) -> Bundle? {
  return bundleCache.use { cache -> Bundle? in
    switch cache[path] {
    case .none:
      break
    case .some(let bundle):
      return bundle
    }

    let bundle = getBundleDirect(containing: path)

    cache[path] = bundle

    return bundle
  }
}

func getBundleDirect(containing path: URL) -> Bundle? {
  if let bundle = Bundle(path: path.path) { return bundle }
  
  let pathComponents = path.pathComponents
  
  guard pathComponents.suffix(4).dropFirst().dropLast() == ["Contents", "MacOS"] else { return nil }
  
  guard let bundle = Bundle(path: pathComponents.dropLast(3).joined(separator: "/")) else { return nil }
  
  guard bundle.executableURL?.lastPathComponent == pathComponents.last else { return nil }
  
  return bundle
}

private var bundleCache = Monitor<[URL: Bundle?]>([:])

func getBundleIcon(_ bundle: Bundle?) -> NSImage? {
  if let iconName = bundle?.infoDictionary?["CFBundleIconFile"] as? String {
    return bundle?.image(forResource: NSImage.Name(iconName))
  }
  return nil
}
