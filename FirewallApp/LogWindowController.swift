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
// Created by Alexey Antipov on 09.09.2019.
//

import AppKit

class LogWindowController: NSWindowController {
  static func show() -> LogWindowController {
    let controller = LogWindowController()
    controller.window?.makeKeyAndOrderFront(nil)
    controller.window?.center()
    return controller
  }
  
  init() {
    let scrollView = NSTextView.scrollablePlainDocumentContentTextView()
    textView = scrollView.documentView as! NSTextView
    textView.isEditable = false
    
    let window = NSWindow(contentRect: NSRect(origin: .zero, size: CGSize(width: 400, height: 300)),
                          styleMask: [.titled],
                          backing: .buffered,
                          defer: true)
    window.contentView = scrollView
    
    super.init(window: window)
  }
  
  deinit {
    window?.close()
  }
  
  func logMessage(_ logText: String) {
    textView.textStorage?
      .append(NSAttributedString(
        string: "> \(logText)\n",
        attributes: [
          .foregroundColor: NSColor.textColor,
          .font: NSFont.monospacedSystemFont(ofSize: NSFont.systemFontSize, weight: .regular)
      ]))
  }
  
  private let textView: NSTextView
  
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }
}
