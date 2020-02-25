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
// Created by kaz on 11.07.2019.
//

import AppKit
import SwiftUI
import Combine
import NetworkFilter
import nf
import UILib

class Monitor<T> {
    public init(_ value: T) {
        self.value = value
    }
    
    public func use<R>(_ block: (inout T) throws -> R) rethrows -> R {
        sema.wait()
        defer { sema.signal() }
        return try block(&value)
    }
    
    private var value: T
    private var sema = DispatchSemaphore(value: 1)
}

extension FilterResult : Codable {}

extension Binding {
  init<T : AnyObject>(_ object: T, _ keyPath: ReferenceWritableKeyPath<T, Value>) {
    self.init(get: {
      object[keyPath: keyPath]
    }, set: {
      object[keyPath: keyPath] = $0
    })
  }
}

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {
  static let productNameFull = "Firewall by Paragon Software"
  static let productNameShort = "Firewall for Mac"
  static let productNameShortest = "Firewall"
  static let productPolicyAddress = "https://www.paragon-software.com/privacy/"
  static let productTermsOfUseAddress = "https://www.paragon-software.com/terms/"

  func applicationDidFinishLaunching(_ aNotification: Notification) {
    let window = Lazy<NSWindow>(makeMainWindow)
    
    func showProductTour() -> Future<FilterResult, Never> {
      .init { promise in
        let rootView = TourView { mode in
          self.didShowProductTour = true
          promise(.success(mode))
        }
        
        window.value.contentView = NSHostingView(rootView: rootView)
      }
    }
    
    let loadingView = Text("loading network filter…")
      .frame(maxWidth: .greatestFiniteMagnitude, maxHeight: .greatestFiniteMagnitude, alignment: .center)
    
//    let logWindowController = Lazy { LogWindowController.show() }
    
    let logger = { (message: String) in
      print("[activation]:", message)
//      DispatchQueue.main.async { logWindowController.value.logMessage(message) }
    }
    
    let approval = { (needApproval: Bool) in
      DispatchQueue.main.async {
        window.value.contentView = needApproval
          ? NSHostingView(rootView: ExtensionApprovalView().padding())
          : NSHostingView(rootView: loadingView)
      }
    }
    
    func loadRealNetworkFilterManager(mode: FilterResult?) -> AnyPublisher<NetworkFilterManager, Error> {
      activate(extensionInfo: systemExtensionInfo,
               mode: mode ?? .unknownAllow,
               rules: self.savedRules ?? [],
               logger: logger,
               approval: approval)
        .map({ manager in
          mode.map { manager.mode = $0 }
          return manager
        })
        .eraseToAnyPublisher()
    }
    
    let useStub = false
    
    func selectFilterMode() -> AnyPublisher<FilterResult?, Never> {
      didShowProductTour == true
        ? Just(lastFilterMode).eraseToAnyPublisher()
        : showProductTour().map { $0 as FilterResult? }.eraseToAnyPublisher()
    }
    
    func loadNetworkFilterManager(mode: FilterResult?) -> AnyPublisher<NetworkFilterManager, Error> {
      window.value.contentView = NSHostingView(rootView: loadingView)
      
      return useStub
        ? Fail<NetworkFilterManager, Error>(error: NSError(domain: "foo", code: 123)).eraseToAnyPublisher()
//        ? Just(NetworkFilterManagerStub(mode: mode ?? .unknownAllow)).setFailureType(to: Error.self).eraseToAnyPublisher()
        : loadRealNetworkFilterManager(mode: mode)
    }
    
    let bnd = Binding<Bool?>(self, \.suppressBlockDescriptionAlert)
    
    selectFilterMode()
      .setFailureType(to: Error.self)
      .flatMap(loadNetworkFilterManager)
      .receiveLatestOnMainThread()
      .map({ manager in MainState(window: window.value, manager: manager, rulesOptions: self.rulesOptions ?? .showAll, rulesSort: self.rulesSort ?? .activeFirst, suppressBlockAlert: bnd, accessCallback: self.showCheckAccessWindow) })
      .sinkNoCancel(receiveCompletion: { completion in
        if case let .failure(error) = completion {
          NSAlert(error: error).runModal()
          NSApp.terminate(nil)
        }
      }, receiveValue: self.setMainState)
  }

  func applicationWillTerminate(_ aNotification: Notification) {
    // Insert code here to tear down your application
  }
  
  func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
    guard !flag, let window = mainState?.window else { return true }
    window.makeKeyAndOrderFront(nil)
    NSApp.activate(ignoringOtherApps: true)
    return false
  }
  
  func applicationShouldTerminate(_ sender: NSApplication) -> NSApplication.TerminateReply {
    disableNetworkExtension(extensionIdentifier: systemExtensionInfo.identifier)
      .sinkNoCancel(receiveCompletion: { _ in
        self.mainState.map { mainState in
          self.lastFilterMode = mainState.filterModel.filterMode.filterResult
          self.savedRules = mainState.filterModel.appsInfo.map { $0.rule }
          self.rulesOptions = mainState.filterModel.rulesOptions
          self.rulesSort = mainState.filterModel.rulesSort
        }
        NSApp.reply(toApplicationShouldTerminate: true)
      }, receiveValue: { })
    
    return .terminateLater
  }
  
  private struct MainState {
    let window: NSWindow
    let filterModel: NetFilterModel
    let toolbarDelegate: ToolbarDelegate
    let statusBarManager: StatusBarMenuManager
    
    init(window: NSWindow, manager: NetworkFilterManager, rulesOptions: RulesOptions, rulesSort: RulesSort, suppressBlockAlert: Binding<Bool?>, accessCallback: @escaping NetFilterModel.AskAccessCallback) {
      let filterModel = NetFilterModel(networkFilterManager: manager, rulesOptions: rulesOptions, rulesSort: rulesSort) { packet, permission in
        DispatchQueue.main.async { accessCallback(packet, permission) }
      }
      
      window.styleMask = [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView, .unifiedTitleAndToolbar]
      window.titleVisibility = .hidden
      window.titlebarAppearsTransparent = false
      window.isReleasedWhenClosed = false
      window.contentView = NSHostingView(rootView: ContentView().environmentObject(filterModel))
      
      filterModel.replaceRuleHandler = { rule in
        guard rule.permission == .deny, suppressBlockAlert.wrappedValue != true else { return }
        
        let alert = NSAlert()
        alert.messageText = ""
        alert.informativeText = "Please, note that you must restart the application for the changes to take effect."
        alert.showsSuppressionButton = true
        alert.beginSheetModal(for: window) { _ in
          if alert.suppressionButton?.state == .on {
            suppressBlockAlert.wrappedValue = true
          }
        }
      }
      
      let toolbarDelegate = ToolbarDelegate(model: filterModel, window: window)
      window.toolbar = toolbarDelegate.toolbar
      
      self.window = window
      self.filterModel = filterModel
      self.toolbarDelegate = toolbarDelegate
      self.statusBarManager = StatusBarMenuManager(model: filterModel) {
        window.makeKeyAndOrderFront(nil)
      }
    }
  }
  
  private var mainState: MainState?
  private var alertWindows = Set<NSWindow>()
  private lazy var systemExtensionInfo = Bundle.main.systemExtensions[0]
  
  @Defaults("DidShowProductTour")
  private var didShowProductTour: Bool?
  
  @Defaults(rawValue: "LastFilterMode")
  private var lastFilterMode: FilterResult?
  
  @Defaults(json: "Rules")
  private var savedRules: [Rule]?

  @Defaults(rawValue: "RulesOptions")
  var rulesOptions: RulesOptions?
  
  @Defaults(rawValue: "RulesSortOrder")
  var rulesSort: RulesSort?
  
  @Defaults("SuppressBlockDescriptionAlert")
  var suppressBlockDescriptionAlert: Bool?

  @IBAction private func showAbout(_ sender: Any?) {
    NSApp.orderFrontStandardAboutPanel(options: [.applicationName : AppDelegate.productNameFull])
  }

  @IBAction private func terminate(_ sender: Any?) {
    var quitEvent = AppleEvent()
    var quitEventReply = AppleEvent()
    var addressDesc = AEAddressDesc()
    var currentProcessSerial = ProcessSerialNumber(highLongOfPSN: 0, lowLongOfPSN: UInt32(kCurrentProcess))
    
    AECreateDesc(typeProcessSerialNumber, &currentProcessSerial, MemoryLayout.size(ofValue: currentProcessSerial), &addressDesc)
    AECreateAppleEvent(kCoreEventClass,
                       kAEQuitApplication,
                       &addressDesc,
                       AEReturnID(kAutoGenerateReturnID),
                       AETransactionID(kAnyTransactionID),
                       &quitEvent)
    
    AESendMessage(&quitEvent, &quitEventReply, AESendMode(kAENoReply), kAEDefaultTimeout)
    AEDisposeDesc(&quitEventReply)
  }
  
  @IBAction private func showMainWindow(_:Any?) {
    mainState?.window.makeKeyAndOrderFront(nil)
  }
  
  @IBAction private func openAppSupportPage(_:Any?) {
    let supportURL = URL(string: "https://www.paragon-software.com/support/")!
    NSWorkspace.shared.open(supportURL)
  }
  
  private func setMainState(_ state: MainState) {
    self.mainState = state
    
    state.window.makeKeyAndOrderFront(nil)
    NSApp.activate(ignoringOtherApps: true)
  }
  
  private func showCheckAccessWindow(accessInfo application: Application, callback: @escaping (RulePermission) -> Void) {
    let window = makeCheckAccessWindow(application: application) { window, permission in
      self.alertWindows.remove(window)
      window.close()
      callback(permission)
    }

    if let lastWindow = alertWindows.sorted(by: \.orderedIndex).last {
      let frame = lastWindow.frame
      let newPoint = lastWindow.cascadeTopLeft(from: NSPoint(x: frame.minX, y: frame.maxY))
      window.cascadeTopLeft(from: newPoint)
      window.order(.below, relativeTo: lastWindow.windowNumber)
    } else {
      window.makeKeyAndOrderFront(nil)
      window.center()
    }
    
    alertWindows.insert(window)
  }
}

class Lazy<T> {
  init(_ initializer: @escaping () -> T) {
    self.initializer = initializer
  }
  
  private(set) lazy var value = initializer()
  
  private let initializer: () -> T
}

private func makeCheckAccessWindow(application: Application, callback: @escaping (_ window: NSWindow, _ permission: RulePermission) -> Void) -> NSWindow {
  let window = NSWindow(
    contentRect: NSRect(x: 0, y: 0, width: 450, height: 160),
    styleMask: [.titled, .fullSizeContentView],
    backing: .buffered,
    defer: false)

  let rootView = AccessCheckView(applicationInfo: getApplicationInfo(application.path)) { permission in
    callback(window, permission)
  }
  
  window.level = .modalPanel
  window.titlebarAppearsTransparent = true
  window.titleVisibility = .hidden
  window.isReleasedWhenClosed = false
  window.contentView = NSHostingView(rootView: rootView)
  
  return window
}

private func makeMainWindow() -> NSWindow {
  let window = NSWindow(
    contentRect: NSRect(x: 0, y: 0, width: 680, height: 480),
    styleMask: [.titled, .fullSizeContentView],
    backing: .buffered, defer: false)
  
  window.titlebarAppearsTransparent = true
  window.titleVisibility = .hidden
  window.isReleasedWhenClosed = false
  window.tabbingMode = .disallowed
  window.title = "Firewall"
  
  window.center()
  window.makeKeyAndOrderFront(nil)
  NSApp.activate(ignoringOtherApps: true)
  
  window.setFrameAutosaveName("Main Window")
  
  return window
}
