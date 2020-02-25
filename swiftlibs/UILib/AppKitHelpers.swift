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
// Created by Alexey Antipov on 2020-01-23.
//

@_exported import AppKit

public extension NSView {
  func withFrame(width: CGFloat? = nil, minWidth: CGFloat? = nil, maxWidth: CGFloat? = nil, minHeight: CGFloat? = nil, height: CGFloat? = nil) -> Self {
    width.map { widthAnchor.constraint(equalToConstant: $0).isActive = true }
    minWidth.map { widthAnchor.constraint(greaterThanOrEqualToConstant: $0).isActive = true }
    maxWidth.map { widthAnchor.constraint(lessThanOrEqualToConstant: $0).isActive = true }
    minHeight.map { heightAnchor.constraint(greaterThanOrEqualToConstant: $0).isActive = true }
    height.map { heightAnchor.constraint(equalToConstant: $0).isActive = true }

    return self
  }

  func embeddedInScrollView() -> NSScrollView {
    let scrollView = NSScrollView()
    scrollView.documentView = self
    return scrollView
  }

  func embeddedInBox() -> NSBox {
    let box = NSBox()
    box.contentView = self
    return box
  }

  func withContentHuggingPriority(_ priority: NSLayoutConstraint.Priority, for orientation: NSLayoutConstraint.Orientation) -> Self {
    setContentHuggingPriority(priority, for: orientation)
    return self
  }
}

public extension NSObjectProtocol {
  func customized(_ action: (Self) -> Void) -> Self {
    action(self)
    return self
  }
}

public extension NSControl {
  func withFont(_ font: NSFont) -> Self {
    self.font = font
    return self
  }

  func enabled(_ enabled: Bool) -> Self {
    self.isEnabled = enabled
    return self
  }
}

public extension NSControl {
  var callback: (() -> Void)? {
    get {
      guard target === self, action == #selector(self.callbackAction(_:)) else { return nil }
      return objc_getAssociatedObject(self, &NSControl.callbackAssociation) as? (() -> Void)
    }
    set {
      target = self
      action = #selector(self.callbackAction(_:))
      objc_setAssociatedObject(self, &NSControl.callbackAssociation, newValue, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
    }
  }

  @objc private func callbackAction(_: Any?) {
    callback?()
  }

  private static var callbackAssociation = 0
}

@available(OSX 10.12, *)
public extension NSButton {
  convenience init(title: String, callback: @escaping () -> Void) {
    self.init(title: title, target: nil, action: nil)
    self.callback = callback
  }
}

public let applicationName = FileManager.default.displayName(atPath: Bundle.main.bundlePath)
