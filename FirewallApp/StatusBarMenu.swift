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

extension NSImage {
    static func getStatusBarImage(for mode: FilterResult) -> NSImage {
      switch mode {
      case .allAllow, .allDeny:
        fatalError("No icon for this mode")
        
      case .unknownAllow:
        return #imageLiteral(resourceName: "menu-bar-extra-filter-silent-allow")
        
      case .unknownDeny:
        return #imageLiteral(resourceName: "menu-bar-extra-filter-silent-block")
        
      case .wait:
        return #imageLiteral(resourceName: "menu-bar-extra-filter-alert")
      }
    }
  }

class StatusBarMenuManager {
  let model: NetFilterModel
  var cancellables: [AnyCancellable] = []
  var networkFilterStatusMenuItem = NSMenuItem()
  var startstopMenuItem = NSMenuItem()
  var operationModeMenuItem = NSMenuItem()
  var operationModeSubmenu: NSMenu!
  var statusbarItem: NSStatusItem

  init(model: NetFilterModel, openMainWindowCallback: @escaping () -> Void) {
    self.model = model
    self.startstopMenuItem.target = model
    self.statusbarItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
    self.openMainWindowCallback = openMainWindowCallback
    self.operationModeSubmenu = createOperationModeSubmenu()
    self.operationModeMenuItem = NSMenuItem(title: NSLocalizedString("Operation Mode", comment: "status menu"), submenu: operationModeSubmenu)

    let menu = NSMenu()
    
    menu.addItem(operationModeMenuItem)
    
    menu.addItem(NSMenuItem(title: "Open Firewall", target: self, action: #selector(self.showMainWindow(_:)), tag: -1))
    
    menu.addItem(NSMenuItem.separator())

    menu.addItem(networkFilterStatusMenuItem)

    menu.addItem(startstopMenuItem)

    menu.addItem(NSMenuItem.separator())

    menu.addItem(NSMenuItem(title: "Preferences...", action: nil, keyEquivalent: ""))
    
//    menu.addItem(statisticMenuItem)
    
    updateMenuItems()
    
    model.filterModeChanged
      .receiveLatestOnMainThread()
      .sink { [self] _ in self.updateMenuItems() }
      .store(in: &cancellables)

    self.statusbarItem.menu = menu
  }
  
  func updateMenuItems() {
    if case .paused(_) = model.filterMode {
//      networkFilterStatusMenuItem.view = {
//        let stack = NSStackView()
//        stack.orientation = .horizontal
//        stack.addSubview(NSTextField(labelWithString: "Network Filter: Off"))
//        stack.addSubview(NSImageView(image: NSImage(named: NSImage.cautionName)!))
//        stack.autoresizingMask = [.height, .width]
//        stack.translatesAutoresizingMaskIntoConstraints = true
//        return stack
//        }()
      networkFilterStatusMenuItem.title = "Network Filter: Off"
      
      startstopMenuItem.title = "Start Network Filter"
      startstopMenuItem.action = #selector(model.resumeFilter(_:))
      
      operationModeMenuItem.isEnabled = false
      
      statusbarItem.button?.image = #imageLiteral(resourceName: "menu-bar-extra-filter-off")
    }
    else {
//      networkFilterStatusMenuItem.view = nil
      networkFilterStatusMenuItem.title = "Network Filter: On"
      
      startstopMenuItem.title = "Stop Network Filter"
      startstopMenuItem.action = #selector(model.stopFilter(_:))

      operationModeMenuItem.isEnabled = true

      statusbarItem.button?.image = NSImage.getStatusBarImage(for: model.filterMode.filterResult)
    }
    
    FilterResult.allCases.forEach { mode in
      self.operationModeSubmenu.item(withTag: mode.tag)?.state = self.model.filterMode.filterResult.tag == mode.tag ? .on : .off
    }
  }
  
  func createOperationModeSubmenu() -> NSMenu {
    let modesSubmenu = NSMenu()
    
    modesSubmenu.addItem({
      let menuItem = NSMenuItem()
      menuItem.title = "Alert mode"
      menuItem.image = NSImage.getImage(for: .wait)
      menuItem.action = #selector(model.setFilterNewAsked(_:))
      menuItem.target = model
      menuItem.tag  = FilterResult.wait.tag
      return menuItem
      }())
    
    modesSubmenu.addItem(NSMenuItem.separator())

    modesSubmenu.addItem({
      let menuItem = NSMenuItem()
      menuItem.title = "Silent Mode - All new apps allowed"
      menuItem.image = NSImage.getImage(for: .unknownAllow)
      menuItem.action = #selector(model.setFilterNewAllowed(_:))
      menuItem.target = model
      menuItem.tag = FilterResult.unknownAllow.tag
      return menuItem
      }())
    
    modesSubmenu.addItem({
      let menuItem = NSMenuItem()
      menuItem.title = "Silent mode - All new apps blocked"
      menuItem.image = NSImage.getImage(for: .unknownDeny)
      menuItem.action = #selector(model.setFilterNewBlocked(_:))
      menuItem.target = model
      menuItem.tag = FilterResult.unknownDeny.tag
      return menuItem
      }())

    return modesSubmenu
  }
  
  @objc func toggleStatistic(_ sender: NSMenuItem) {
    model.statisticEnabled.toggle()
    statisticMenuItem.title = statisticItemTitle
  }
  
  @objc func showMainWindow(_ : Any?){
    openMainWindowCallback()
  }
  
  private var statisticItemTitle: String {
    model.statisticEnabled ? "Disable statistic" : "Enable statistic"
  }
  
  private let openMainWindowCallback: () -> Void
  
  lazy var statisticMenuItem: NSMenuItem = {
    let item = NSMenuItem(title: statisticItemTitle, action: #selector(self.toggleStatistic(_:)), keyEquivalent: "")
    item.target = self
    return item
  }()
}

extension FilterResult : CaseIterable {
  public static var allCases: [FilterResult] = [.allAllow, .allDeny, .unknownAllow, .unknownDeny, .wait]
  var tag: Int {
    guard let index = FilterResult.allCases.firstIndex(of: self) else { fatalError() }
    return index + 10
  }
}
