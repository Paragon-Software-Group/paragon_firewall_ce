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

import AppKit

extension NSButton {
  public static func linkButton(title: String, opensURL url: URL? = nil) -> NSButton {
    let button = NSButton()
    button.cell = LinkButtonCell().customized { $0.awakeFromNib() }
    button.title = title
    url.map { url in
      button.callback = {
        NSWorkspace.shared.open(url)
      }
    }
    return button
  }
}

public class LinkButtonCell: NSButtonCell {
  public var textColor: NSColor = NSColor.linkColor {
    didSet {
      controlView?.needsDisplay = true
    }
  }

  public override func awakeFromNib() {
    showsBorderOnlyWhileMouseInside = true
    super.awakeFromNib()
  }

  public override func draw(withFrame cellFrame: NSRect, in _: NSView) {
    let title = self.title as NSString

    var attributes: [NSAttributedString.Key: Any] = [
      .font: self.font!,
      .foregroundColor: textColor,
    ]

    if isMouseInside {
      attributes[.underlineStyle] = 1
    }

    let titleSize = title.size(withAttributes: attributes)
    var rect: NSRect = NSRect(origin: NSZeroPoint, size: titleSize)

    rect.origin.x = floor(cellFrame.minX + (cellFrame.width - titleSize.width) / 2)
    rect.origin.y = floor(cellFrame.minY + (cellFrame.height - titleSize.height) / 2)

    NSGraphicsContext.saveGraphicsState()
    title.draw(in: rect, withAttributes: attributes)
    NSGraphicsContext.restoreGraphicsState()
  }

  public override func mouseEntered(with _: NSEvent) {
    NSCursor.pointingHand.set()
    isMouseInside = true

    controlView?.needsDisplay = true
  }

  public override func mouseExited(with _: NSEvent) {
    NSCursor.arrow.set()
    isMouseInside = false

    controlView?.needsDisplay = true
  }

  private var isMouseInside: Bool = false
}
