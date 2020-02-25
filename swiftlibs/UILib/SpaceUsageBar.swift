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
// Created by Alexey Antipov on 27/02/2019.
//

import AppKit

class SpaceUsageBar: NSView {
  var usageRatio: CGFloat = 0 {
    didSet {
      needsLayout = true
    }
  }

  init() {
    super.init(frame: NSRect.zero)

    let layer = CALayer()
    layer.backgroundColor = NSColor(red: 189 / 255, green: 198 / 255, blue: 208 / 255, alpha: 0.5).cgColor
    layer.sublayers = [colorLayer]
    layer.cornerRadius = 1.5

    self.layer = layer
  }

  required init?(coder _: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override var wantsUpdateLayer: Bool {
    return true
  }

  override func layout() {
    super.layout()

    let usageRatio = self.usageRatio.isNaN ? 0 : self.usageRatio

    colorLayer.frame = CGRect(
      x: 0,
      y: 0,
      width: bounds.width * usageRatio,
      height: bounds.height
    )
  }

  private lazy var colorLayer: CALayer = {
    let layer = CALayer()
    layer.backgroundColor = NSColor(hex: 0x499EFF).cgColor
    return layer
  }()

  override var intrinsicContentSize: NSSize {
    return NSSize(width: NSView.noIntrinsicMetric, height: 3)
  }
}
