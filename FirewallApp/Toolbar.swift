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
// Created by kaz on 14.08.2019.
//

import AppKit
import Cocoa
import Combine
import NetworkFilter
import nf

extension NSToolbarItem.Identifier {
  static let startStop = NSToolbarItem.Identifier("startStop")
  static let mode = NSToolbarItem.Identifier("mode")
  static let add = NSToolbarItem.Identifier("add")
  static let filterOrder = NSToolbarItem.Identifier("filterOrder")
  static let search = NSToolbarItem.Identifier("search")
}

class ToolbarDelegate: NSObject {
  var cancellables: [AnyCancellable] = []
  let filterModel: NetFilterModel
  let window: NSWindow
  
  init(model: NetFilterModel, window: NSWindow) {
    self.filterModel = model
    self.window = window
  }

  lazy var toolbarItems: [NSToolbarItem.Identifier:NSToolbarItem] = [
    .add : makeToolbarItemAddApp(id: .add),
    .startStop : makeToolbarItemStartStop(id: .startStop),
    .mode : makeToolbarItemMode(id: .mode),
    .filterOrder : NSToolbarItem(itemIdentifier: .filterOrder, view: self.buttonMenuFilterOrder),
    .search : makeToolbarItemSearch(id: .search)
  ]
  
  lazy var toolbar: NSToolbar = {
    let toolbar = NSToolbar(identifier: "FirewallToolbar")
    toolbar.allowsUserCustomization = false
    toolbar.displayMode = .iconOnly
    toolbar.delegate = self
    return toolbar
  }()
  
  lazy var menuFilterOrder: NSMenu = {
    let menu = NSMenu()
    menu.showsStateColumn = true
    menu.addItem({
      let menuItem = NSMenuItem(title: "Show", action: nil, keyEquivalent: "")
      menuItem.view = {
        let text = NSTextField(labelWithString: NSLocalizedString("Show", comment: "toolbar filter for shown apps"))
        text.font = NSFont.systemFont(ofSize: 13, weight: .light)
        return text
      }()
      return menuItem
    }())
    
    let options: [(RulesOptions, String)] = [
      (.showAll, NSLocalizedString("All apps", comment: "")),
      (.showAllow, NSLocalizedString("Allowed apps", comment: ""))	,
      (.showDeny, NSLocalizedString("Blocked apps", comment: ""))
    ]
    for (option, title) in options {
      menu.addItem({
        let menuItem = NSMenuItem(title: title, target: self, action: #selector(switchFilter), tag: option.rawValue)
        if self.filterModel.rulesOptions == option { menuItem.state = .on }
        return menuItem
      }())
    }
    menu.addItem(NSMenuItem.separator())
    menu.addItem({
      let menuItem = NSMenuItem(title: "Show", action: nil, keyEquivalent: "")
      menuItem.view = {
        let text = NSTextField(labelWithString: NSLocalizedString("Sort by", comment: "toolbar filter for shown apps"))
        text.font = NSFont.systemFont(ofSize: 13, weight: .light)
        return text
      }()
      return menuItem
    }())
    for order in RulesSort.allCases {
      menu.addItem({
        let menuItem = NSMenuItem(title: order.title, target: self, action: #selector(switchOrder), tag: order.tag)
        if self.filterModel.rulesSort == order { menuItem.state = .on }
        return menuItem
        }())
    }
    return menu
  }()
  
  lazy var buttonMenuFilterOrder: NSButton = {
    let button = NSButton()
    button.image = #imageLiteral(resourceName: "ico-filter-default")
    button.target = self
    button.action = #selector(showFilterOrderMenu(_:))
    button.bezelStyle = .regularSquare
    button.isBordered = false
    return button
  }()
  
  func makeToolbarItemAddApp(id: NSToolbarItem.Identifier) -> NSToolbarItem {
    let button = NSButton()
    button.target = self
    button.bezelStyle = .texturedRounded
    button.widthAnchor.constraint(equalToConstant: 34).isActive = true
    button.image = NSImage(named: NSImage.addTemplateName)!
    button.action = #selector(self.addApplication)
    return NSToolbarItem(itemIdentifier: id, view: button)
  }

  func makeToolbarItemStartStop(id: NSToolbarItem.Identifier) -> NSToolbarItem {
    let button = NSButton()
    button.target = filterModel
    button.bezelStyle = .texturedRounded
    button.widthAnchor.constraint(equalToConstant: 34).isActive = true

    let updateButtonWithMode: (NetFilterModel.FilterMode) -> Void = { mode in
      if case .paused(_) = mode {
        button.image = #imageLiteral(resourceName: "ico-start")
        button.action = #selector(NetFilterModel.resumeFilter)
      }
      else {
        button.image = NSImage(named: NSImage.touchBarRecordStopTemplateName)
        button.action = #selector(NetFilterModel.stopFilter(_:))
      }
    }
    
    updateButtonWithMode(self.filterModel.filterMode)
      
    self.filterModel.filterModeChanged
      .receiveLatestOnMainThread()
      .sink { updateButtonWithMode($0) }
      .store(in: &cancellables)
    
    return NSToolbarItem(itemIdentifier: id, view: button)
  }
  
  func makeToolbarItemMode(id: NSToolbarItem.Identifier) -> NSToolbarItem {
    let menu = NSMenu()
    menu.showsStateColumn = false
    let segmentedControl = NSSegmentedControl()

    func addItem(title: String, image: NSImage, target: AnyObject, action: Selector) {
      let item = menu.addItem(withTitle: title, action: action, keyEquivalent: "")
      item.image = image
      item.target = target
    }
    
    func addaptToMode(filterMode: NetFilterModel.FilterMode) {
      switch filterMode {
      case .paused(let previousMode):
        segmentedControl.isEnabled = false
        segmentedControl.setImage(NSImage.getImage(for: previousMode), forSegment: 0)

      case .running(let mode):
        segmentedControl.isEnabled = true
        segmentedControl.setImage(NSImage.getImage(for: mode), forSegment: 0)
      }
    }
    
    addItem(title: "Silent mode - All new apps allowed",
            image: NSImage.getImage(for: .unknownAllow),
            target: filterModel,
            action: #selector(filterModel.setFilterNewAllowed(_:)))
    
    addItem(title: "Silent mode - All new apps blocked",
            image: NSImage.getImage(for: .unknownDeny),
            target: filterModel,
            action: #selector(filterModel.setFilterNewBlocked(_:)))
    
    addItem(title: "Alert mode - Get alerted on new apps",
            image: NSImage.getImage(for: .wait),
            target: filterModel,
            action: #selector(filterModel.setFilterNewAsked(_:)))

    segmentedControl.segmentCount = 2
    segmentedControl.trackingMode = .momentary
    segmentedControl.setWidth(30, forSegment: 0)
    segmentedControl.setWidth(16, forSegment: 1)
    
    addaptToMode(filterMode: filterModel.filterMode)
    
    segmentedControl.setMenu(menu, forSegment: 1)
    segmentedControl.setShowsMenuIndicator(true, forSegment: 1)
    
    self.filterModel.filterModeChanged
      .receiveLatestOnMainThread()
      .sink(receiveValue: addaptToMode)
      .store(in: &cancellables)
    
    return NSToolbarItem(itemIdentifier: id, view: segmentedControl)
  }
  
  func makeToolbarItemSearch(id: NSToolbarItem.Identifier) -> NSToolbarItem {
    let search = NSSearchField()
    search.sendsSearchStringImmediately = false
    search.target = self
    search.action = #selector(applySearchFilter(_:))
    search.cell?.isScrollable = true
    return NSToolbarItem(itemIdentifier: id, view: search)
  }
  
  @objc func addApplication(_: Any) {
    let openPanel = NSOpenPanel()
    openPanel.treatsFilePackagesAsDirectories = true
    openPanel.canChooseDirectories = false
    openPanel.canChooseFiles = true
    openPanel.allowsMultipleSelection = false
    openPanel.canCreateDirectories = false
    openPanel.directoryURL = URL(fileURLWithPath: "/Applications/", isDirectory: true)
    openPanel.prompt = NSLocalizedString("Choose", comment: "")
    openPanel.treatsFilePackagesAsDirectories = false
    openPanel.delegate = self
    openPanel.beginSheetModal(for: window) { [weak self] response in
      guard response == .OK, let url = openPanel.url else { return }
      self?.filterModel.addApplication(path: url)
    }
  }
  
  @objc func switchFilter(_ sender: Any) {
    guard let menuItem = sender as? NSMenuItem else { fatalError() }
    self.filterModel.rulesOptions = RulesOptions(rawValue: menuItem.tag)
    RulesOptions.allCases.forEach { filterRule in
      self.menuFilterOrder.item(withTag: filterRule.rawValue)?.state = menuItem.tag == filterRule.rawValue ? .on : .off
    }
    self.buttonMenuFilterOrder.image = menuItem.tag == RulesOptions.showAll.rawValue ? #imageLiteral(resourceName: "ico-filter-default") : #imageLiteral(resourceName: "ico-filter-modified")
  }
  
  @objc func switchOrder(_ sender: Any) {
    guard let menuItem = sender as? NSMenuItem else { fatalError() }
    guard let sortRule = RulesSort.caseBy(tag: menuItem.tag) else { fatalError() }
    self.filterModel.rulesSort = sortRule
    RulesSort.allCases.forEach { sortRule in
      self.menuFilterOrder.item(withTag: sortRule.tag)?.state = sortRule.tag == menuItem.tag ? .on : .off
    }
  }
  
  @objc func showFilterOrderMenu(_ sender: Any) {
    guard let view = sender as? NSView else {
      fatalError("showFilterOrderMenu sender is not NSView")
    }
    self.menuFilterOrder.popUp(positioning: nil, at: NSPoint(x: 0, y: view.bounds.height+10), in: view)
  }
  
  @objc func applySearchFilter(_ searchField: NSSearchField) {
    filterModel.rulesQueryApplicationMask = searchField.stringValue.isEmpty ? nil : searchField.stringValue
  }
}

extension ToolbarDelegate : NSToolbarDelegate {
  func toolbar(_ toolbar: NSToolbar, itemForItemIdentifier itemIdentifier: NSToolbarItem.Identifier, willBeInsertedIntoToolbar flag: Bool) -> NSToolbarItem? {
    return self.toolbarItems[itemIdentifier]
  }
  
  func toolbarDefaultItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
    return [.add, .startStop, .mode, .flexibleSpace, .filterOrder, .search]
  }
  
  func toolbarAllowedItemIdentifiers(_ toolbar: NSToolbar) -> [NSToolbarItem.Identifier] {
    return self.toolbarDefaultItemIdentifiers(toolbar)
  }
}

extension ToolbarDelegate : NSOpenSavePanelDelegate {
  func panel(_ sender: Any, shouldEnable url: URL) -> Bool {
    if url.hasDirectoryPath { return true }
    guard let resources = try? url.resourceValues(forKeys: [.typeIdentifierKey]),
      let uti = resources.typeIdentifier
      else { return false }
    return UTTypeConformsTo(uti as CFString, kUTTypeApplicationBundle) || UTTypeConformsTo(uti as CFString, kUTTypeExecutable)
  }
}

extension RulesOptions: CaseIterable {
  public static var allCases: [RulesOptions] = [.showAllow, .showDeny, .showAll]
}

extension RulesSort: CaseIterable {
  public static var allCases: [RulesSort] = [.activeFirst, .activeLast, .appNameAZ, .appNameZA]
  
  var tag: Int {
    let base = 10
    switch self {
    case .activeFirst: return base + 0
    case .activeLast: return base + 1
    case .appNameAZ: return base + 2
    case .appNameZA: return base + 3
    }
  }
  
  static func caseBy(tag: Int) -> Self? {
    switch tag {
    case Self.activeFirst.tag: return .activeFirst
    case Self.activeLast.tag: return .activeLast
    case Self.appNameAZ.tag: return .appNameAZ
    case Self.appNameZA.tag: return .appNameZA
    default: return nil
    }
  }
}

extension RulesSort {
  var title: String {
    switch self {
    case .activeFirst: return NSLocalizedString("Date (recent on top)", comment: "")
    case .activeLast: return NSLocalizedString("Date (oldest on top)", comment: "")
    case .appNameAZ: return NSLocalizedString("Name (A-z)", comment: "")
    case .appNameZA: return NSLocalizedString("Name (Z-a)", comment: "")
    }
  }
}
