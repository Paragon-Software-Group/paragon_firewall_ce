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
// Created by Alexey Antipov on 2019-12-11.
//

import AppKit

public extension NSTextField {
  @available(OSX, obsoleted: 10.12)
  convenience init(labelWithString string: String) {
    self.init(frame: NSRect.zero)

    stringValue = string
    isBordered = false
    drawsBackground = false
    isEditable = false
    isSelectable = false
  }

  @available(OSX, obsoleted: 10.12)
  convenience init(wrappingLabelWithString string: String) {
    self.init(frame: NSRect.zero)

    stringValue = string
    isBordered = false
    drawsBackground = false
    isEditable = false
    isSelectable = false
    lineBreakMode = .byWordWrapping
  }
}

public extension NSColor {
  convenience init(hex color: Int) {
    self.init(calibratedRed: CGFloat((color >> 16) & 0xFF) / 255.0, green: CGFloat((color >> 8) & 0xFF) / 255.0, blue: CGFloat(color & 0xFF) / 255.0, alpha: 1)
  }
}
