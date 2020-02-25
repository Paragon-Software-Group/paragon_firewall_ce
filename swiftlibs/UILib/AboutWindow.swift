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

import Foundation

@available(OSX 10.12, *)
public func makeAboutWindow() -> NSWindow {
  let bundleInfo = Bundle.main.infoDictionary ?? [:]

  let versionString = { () -> String? in
    let versionString = bundleInfo["CFBundleShortVersionString"] as? String
    let buildNumberString = bundleInfo[kCFBundleVersionKey as String] as? String
    if let version = versionString {
      return buildNumberString.map { "\(version) (\($0))" } ?? version
    }
    return buildNumberString
  }()

  let copyrightString = bundleInfo["NSHumanReadableCopyright"] as? String

  let makeSeparator: () -> NSView = {
    NSBox()
      .withFrame(height: 15)
      .customized { $0.boxType = .separator }
  }

  let view = NSStackView(views: [
    NSImageView(image: NSImage(named: NSImage.applicationIconName)!)
      .withFrame(width: 96, height: 96),

    NSTextField(labelWithString: applicationName)
      .withFont(.systemFont(ofSize: 16)),

    NSTextField(labelWithString: "Version \(versionString ?? "(Unknown)")"),

    NSTextField(labelWithString: copyrightString ?? ""),

    NSStackView(views: [
      NSButton.linkButton(
        title: "Privacy Policy",
        opensURL: URL(string: "https://www.paragon-software.com/privacy.htm")
      ),

      makeSeparator(),

      NSButton.linkButton(
        title: "Webpage",
        opensURL: URL(string: "https://www.paragon-software.com")
      ),

      makeSeparator(),

      NSButton.linkButton(
        title: "Customer Support",
        opensURL: URL(string: "https://www.paragon-software.com/support")
      ),
    ]),
  ])
  view.orientation = .vertical
  view.edgeInsets = NSEdgeInsets(top: 20, left: 10, bottom: 16, right: 10)
  view.spacing = 16

  let window = NSWindow(
    contentRect: NSRect(x: 0, y: 0, width: 430, height: 270),
    styleMask: [.titled, .closable],
    backing: .buffered,
    defer: false
  )

  window.title = "About \(applicationName)"
  window.contentView = view
  window.isReleasedWhenClosed = false

  return window
}

@available(OSX 10.12, *)
@discardableResult
public func showAboutWindow() -> NSWindow {
  globalAboutWindow.makeKeyAndOrderFront(nil)
  globalAboutWindow.center()
  return globalAboutWindow
}

@available(OSX 10.12, *)
private var globalAboutWindow: NSWindow = makeAboutWindow()
