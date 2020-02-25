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

public class HighlightView: NSView {
  public let inset: CGFloat = 10

  public var callback: (() -> Void)? {
    didSet {
      if callback == nil {
        isHighlighted = false
      }
      isEnabled = callback != nil
    }
  }

  public var isEnabled = false

  public init(subview: NSView) {
    self.subview = subview

    super.init(frame: NSRect.zero)

    wantsLayer = true
    layerContentsRedrawPolicy = .onSetNeedsDisplay

    layer?.addSublayer(highlightingSublayer)

    subview.translatesAutoresizingMaskIntoConstraints = false
    subviews = [subview]
    subview.leftAnchor.constraint(equalTo: leftAnchor, constant: inset).isActive = true
    subview.topAnchor.constraint(equalTo: topAnchor, constant: inset).isActive = true
    rightAnchor.constraint(equalTo: subview.rightAnchor, constant: inset).isActive = true
    bottomAnchor.constraint(equalTo: subview.bottomAnchor, constant: inset).isActive = true
  }

  required init?(coder _: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  private func insetIntrinsicDiminesion(_ value: CGFloat) -> CGFloat {
    return value == NSView.noIntrinsicMetric ? value : value + inset * 2
  }

  public override var intrinsicContentSize: NSSize {
    let subviewSize = subview.intrinsicContentSize
    return NSSize(
      width: insetIntrinsicDiminesion(subviewSize.width),
      height: insetIntrinsicDiminesion(subviewSize.height)
    )
  }

  public override func acceptsFirstMouse(for _: NSEvent?) -> Bool {
    return false
  }

  public override func updateTrackingAreas() {
    trackingArea = NSTrackingArea(
      rect: bounds,
      options: [.activeAlways, .mouseEnteredAndExited],
      owner: self,
      userInfo: nil
    )
  }

  public override func mouseEntered(with _: NSEvent) {
    if callback != nil, isEnabled {
      isHighlighted = true
    }
  }

  public override func mouseExited(with _: NSEvent) {
    isHighlighted = false
  }

  public override func mouseDown(with _: NSEvent) {
    if isEnabled {
      callback?()
    }
  }

  public override func updateLayer() {
    highlightingSublayer.frame = bounds

    guard isHighlighted else {
      highlightingSublayer.isHidden = true
      return
    }

    highlightingSublayer.isHidden = false
  }

  private lazy var highlightingSublayer: CAGradientLayer = {
    let layer = CAGradientLayer()
    layer.borderWidth = 1
    layer.cornerRadius = 5
    layer.masksToBounds = true
    layer.borderColor = NSColor(calibratedWhite: 1, alpha: 0.5).cgColor
    layer.colors = [
      NSColor(red: 0, green: 118 / 255, blue: 213 / 255, alpha: 0.3).cgColor,
      NSColor(red: 79 / 255, green: 176 / 255, blue: 1, alpha: 0.5).cgColor,
    ]
    return layer
  }()

  private let subview: NSView

  private var trackingArea: NSTrackingArea? {
    didSet {
      if let old = oldValue {
        removeTrackingArea(old)
      }
      if let new = trackingArea {
        addTrackingArea(new)
      }
    }
  }

  private var isHighlighted: Bool = false {
    didSet {
      needsDisplay = true
    }
  }
}
