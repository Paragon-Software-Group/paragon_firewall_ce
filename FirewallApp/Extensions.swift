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
import nf
import Combine

extension NSImage {
  static func getImage(for mode: FilterResult) -> NSImage {
    switch mode {
    case .allAllow, .allDeny:
      fatalError("No icon for this mode")
      
    case .unknownAllow:
      return #imageLiteral(resourceName: "ico-silent-new-allowed")
      
    case .unknownDeny:
      return #imageLiteral(resourceName: "ico-silent-new-blocked")
      
    case .wait:
      return #imageLiteral(resourceName: "ico-alerts-on")
    }
  }
}

extension NSToolbarItem {
  convenience init(itemIdentifier: NSToolbarItem.Identifier, view: NSView) {
    self.init(itemIdentifier: itemIdentifier)
    self.view = view
  }
}

extension NSMenuItem {
  convenience init(title: String, target: AnyObject?, action: Selector?, tag: Int) {
    self.init(title: title, action: action, keyEquivalent: "")
    self.target = target
    self.tag = tag
  }

  convenience init(title: String, submenu: NSMenu) {
    self.init(title: title, action: nil, keyEquivalent: "")
    self.submenu = submenu
  }
}

extension Publisher {
  func sinkNoCancel(receiveCompletion: @escaping ((Subscribers.Completion<Failure>) -> Void), receiveValue: @escaping ((Output) -> Void)) {
    var cancellable: AnyCancellable?
    
    cancellable = sink(receiveCompletion: { completion in
      _ = cancellable
      receiveCompletion(completion)
    }, receiveValue: { value in
      _ = cancellable
      receiveValue(value)
    })
  }
}

extension Sequence {
  func sorted<T>(by keyPath: KeyPath<Element, T>) -> [Element] where T : Comparable {
    sorted(by: { $0[keyPath: keyPath] < $1[keyPath: keyPath] })
  }
}

extension Publisher {
  func receiveLatestOnMainThread() -> Publishers.ReceiveOn<Publishers.Buffer<Self>, RunLoop> {
    buffer(size: 1, prefetch: .byRequest, whenFull: .dropOldest)
      .receive(on: RunLoop.main)
  }
}
