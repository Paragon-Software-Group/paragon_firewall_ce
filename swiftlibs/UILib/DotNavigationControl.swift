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
// Created by Alexey Antipov on 12/05/17.
//

import Cocoa

@IBDesignable
public class DotNavigationControl: NSControl {
  @IBInspectable public var dotsCount: UInt = 0 {
    didSet {
      setupLayers()
      updateFrames()
    }
  }

  @objc public dynamic var selectedDot: UInt = 0 {
    didSet {
      guard oldValue != selectedDot else { return }

      sublayers[Int(oldValue)].backgroundColor = kRegularDotColor
      sublayers[Int(selectedDot)].backgroundColor = kSelectedDotColor

      updateFrames()
    }
  }

  private let kSelectedDotColor = NSColor(calibratedRed: 0, green: 0.4471, blue: 0.9725, alpha: 1).cgColor
  private let kRegularDotColor = NSColor(calibratedWhite: 0.6078, alpha: 1).cgColor

  private let kDotRadius: CGFloat = 5
  private let mainLayer: CALayer = CALayer()
  private var sublayers: [CALayer] = []

  required init?(coder: NSCoder) {
    super.init(coder: coder)

    setupLayers()
    updateFrames()
  }

  override init(frame frameRect: NSRect) {
    super.init(frame: frameRect)

    setupLayers()
    updateFrames()
  }

  public override func layout() {
    updateFrames()
  }

  private func setupLayers() {
    sublayers = (0 ..< dotsCount).map { index in
      let dotLayer = CALayer()
      dotLayer.backgroundColor = index == selectedDot ? kSelectedDotColor : kRegularDotColor
      mainLayer.addSublayer(dotLayer)
      return dotLayer
    }

    layer = mainLayer
    wantsLayer = true
  }

  private func updateFrames() {
    let dotY: CGFloat = bounds.minY + bounds.height / 2 - kDotRadius
    let maxDotX: CGFloat = bounds.width - kDotRadius * 2
    let dotSize = CGSize(width: kDotRadius * 2, height: kDotRadius * 2)

    for (index, dotLayer) in sublayers.enumerated() {
      let dotOrigin = CGPoint(x: index > 0 ? floor(maxDotX / CGFloat(dotsCount - 1) * CGFloat(index)) : 0, y: dotY)
      let dotBounds = CGRect(origin: dotOrigin, size: dotSize)

      let selected = (Int(selectedDot) == index)
      dotLayer.frame = selected ? dotBounds : dotBounds.insetBy(dx: 1, dy: 1)
      dotLayer.cornerRadius = selected ? kDotRadius : kDotRadius - 1
    }
  }

  public override func mouseDown(with theEvent: NSEvent) {
    super.mouseDown(with: theEvent)

    let mousePoint = convert(theEvent.locationInWindow, from: nil)

    for (index, dotLayer) in sublayers.enumerated() {
      if NSPointInRect(mousePoint, dotLayer.frame) {
        selectedDot = UInt(index)
        break
      }
    }
  }

  public override var acceptsFirstResponder: Bool {
    return true
  }

  public override func moveLeft(_: Any?) {
    if selectedDot > 0 {
      selectedDot -= 1
    }
  }

  public override func moveRight(_: Any?) {
    if selectedDot < dotsCount - 1 {
      selectedDot += 1
    }
  }
}
