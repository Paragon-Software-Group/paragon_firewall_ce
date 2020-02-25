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
import SwiftUI
import Combine
import os.log
import NetworkFilter
import nf

class NetFilterModel: ObservableObject {
  typealias AskAccessCallback = (Application, @escaping (RulePermission) -> Void) -> Void
  
  @Published
  private(set) var appsInfo: [MonitoredAppModel] = []
  
  var rulesOptions: RulesOptions {
    didSet { objectWillChange.send(); updateRules(completion: nil) }
  }
  
  var rulesSort: RulesSort {
    didSet { objectWillChange.send(); updateRules(completion: nil) }
  }
  
  var rulesQueryApplicationMask: String? {
    didSet { objectWillChange.send(); updateRules(completion: nil) }
  }
  
  enum FilterMode {
    case paused(previousMode: FilterResult)
    case running(FilterResult)

    var filterResult: FilterResult {
      switch self {
      case .running(let mode): return mode
      case .paused: return .allAllow
      }
    }
  }
  
  var filterMode: FilterMode {
    didSet {
      networkFilterManager.mode = filterMode.filterResult
      filterModeChanged.send(filterMode)
      if case .paused(let mode) = filterMode {
        UserDefaults.standard.set(mode.rawValue, forKey: defaultsKeyPausedFilterMode)
      }
      else {
        UserDefaults.standard.removeObject(forKey: defaultsKeyPausedFilterMode)
      }
    }
  }
  
  let filterModeChanged = PassthroughSubject<FilterMode, Never>()
  
  var statisticEnabled: Bool {
    get { networkFilterManager.statisticEnabled }
    set {
      networkFilterManager.statisticEnabled = newValue
      backgroundQueue.async { self.updateStatistic(completion: { }) }
    }
  }
  
  var replaceRuleHandler: ((Rule) -> Void)?

  init(networkFilterManager: NetworkFilterManager, rulesOptions: RulesOptions, rulesSort: RulesSort, accessCallback: @escaping AskAccessCallback) {
    self.networkFilterManager = networkFilterManager
    self.askAccessCallback = accessCallback
    
    let mode = networkFilterManager.mode
    switch mode {
    case .allAllow:
      let savedMode = (UserDefaults.standard.value(forKey: defaultsKeyPausedFilterMode) as? UInt32)
        .flatMap { FilterResult(rawValue: $0) }
      filterMode = .paused(previousMode: savedMode ?? .unknownAllow)
    default:
      filterMode = .running(mode)
    }
    
    self.rulesOptions = rulesOptions
    self.rulesSort = rulesSort
    
    networkFilterManager.registerOnlineAccessChecker { [weak self] in self?.askAccess(application: $0, completion: $1) }
    networkFilterManager.registerRulesUpdateCallback { [weak self] in self?.updateRules(completion: $0) }
    networkFilterManager.registerStatisticUpdateCallback { [weak self] in self?.updateStatistic(completion: $0) }
    
    updateAppsInfo(with: fetchRules())
  }
  
  func deleteRule(withIdentifier id: UInt64) {
    try? networkFilterManager.removeRule(id: id)
  }
  
  func replaceRule(_ rule: Rule) {
    try? networkFilterManager.updateRule(rule)
    replaceRuleHandler?(rule)
  }
  
  func addApplication(path: URL) {
    if case .failure = Result(catching: { try networkFilterManager.addRuleForApplication(at: path.path) }) {
      os_log(.error, "NetworkFilterManager: cannot add rule for application")
    }
  }
  
  func startFilter() {
    guard case let .paused(previous) = filterMode else { return }
    filterMode = .running(previous)
  }
  
  func stopFilter() {
    guard case let .running(mode) = filterMode else { return }
    filterMode = .paused(previousMode: mode)
  }
  
  @objc func stopFilter(_ sender: Any) {
    stopFilter()
  }
  
  @objc func resumeFilter(_ sender: Any) {
    startFilter()
  }
  
  @objc func setFilterNewAllowed(_ sender: Any) {
    setFilterMode(.unknownAllow)
  }
  
  @objc func setFilterNewBlocked(_ sender: Any) {
    setFilterMode(.unknownDeny)
  }
  
  @objc func setFilterNewAsked(_ sender: Any) {
    setFilterMode(.wait)
  }
  
  func setFilterMode(_ mode: FilterResult) {
    guard case .running = filterMode, mode != .allAllow else { return }
    filterMode = .running(mode)
  }
  
  private let defaultsKeyPausedFilterMode = "PausedFilterMode"
  private let networkFilterManager: NetworkFilterManager
  private let askAccessCallback: AskAccessCallback
  private let backgroundQueue = DispatchQueue(label: "NetFilterModel.background", qos: .userInteractive)
  
  private func fetchRules() -> [Rule] {
    networkFilterManager.rules(options: rulesOptions)
  }
  
  private func updateRules(completion: (() -> Void)?) {
    backgroundQueue.async {
      let rules = self.fetchRules()
      
      DispatchQueue.main.async { [weak self] in
        self?.updateAppsInfo(with: rules)
        completion?()
      }
    }
  }
  
  private func askAccess(application: Application, completion: @escaping (RulePermission) -> Void) {
    print("NetFilter callback called \(application.path)")
    askAccessCallback(application, completion)
  }
  
  private func updateAppsInfo(with rules: [Rule]) {
    appsInfo = rules.map { rule in
      if var appInfo = appsInfo.first(where: { $0.rule.id == rule.id }) {
        appInfo.rule = rule
        return appInfo
      }
      
      return MonitoredAppModel(rule: rule, statistic: nil)
    }.filter { model in
      switch rulesQueryApplicationMask {
      case nil:
        return true
      case .some(let mask):
        return model.name.lowercased().starts(with: mask.lowercased())
      }
    }.sorted { lhs, rhs in
      switch rulesSort {
      case .appNameAZ:
        return lhs.name.caseInsensitiveCompare(rhs.name) == .orderedAscending
        
      case .appNameZA:
        return lhs.name.caseInsensitiveCompare(rhs.name) == .orderedDescending
        
      case .activeLast:
        return (lhs.lastAccess ?? Date.distantPast) < (rhs.lastAccess ?? Date.distantPast)
        
      case .activeFirst:
        return (lhs.lastAccess ?? Date.distantPast) > (rhs.lastAccess ?? Date.distantPast)
      }
    }
  }
  
  /// Should  *NOT* be called from main thread: will block.
  private func updateStatistic(completion: @escaping () -> Void) {
    let enabled = DispatchQueue.main.sync { () -> Bool in
      let enabled = statisticEnabled
      if !enabled {
        appsInfo = appsInfo.map { info in
          var info = info
          info.statistic = nil
          return info
        }
      }
      return enabled
    }
    guard enabled else { return }
    
    let applications = DispatchQueue.main.sync { appsInfo.map { $0.rule.application } }
    
    var statistics: [Application: [Statistic]] = [:]
    
    for application in applications {
      statistics[application] = networkFilterManager.statistics(application: application.path) ?? []
    }
    
    DispatchQueue.main.async {
      for (index, appModel) in self.appsInfo.enumerated() {
        statistics[appModel.rule.application].map { self.appsInfo[index].statistic = $0 }
      }
      completion()
    }
  }
}
