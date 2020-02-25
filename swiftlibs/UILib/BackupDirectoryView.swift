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

public class BackupDirectoryView: NSView {
  public var title: String {
    get {
      return titleLabel.stringValue
    }
    set {
      titleLabel.stringValue = newValue
    }
  }

  public var icon: NSImage? {
    get {
      return imageView.image
    }
    set {
      imageView.image = newValue
    }
  }

  public var information: String {
    get {
      return informationLabel.stringValue
    }
    set {
      informationLabel.stringValue = newValue
    }
  }

  public var usageRatio: CGFloat? {
    get {
      return usageBar.isHidden ? nil : usageBar.usageRatio
    }
    set {
      if let value = newValue {
        usageBar.isHidden = false
        usageBar.usageRatio = value
      } else {
        usageBar.isHidden = true
      }
    }
  }

  public private(set) lazy var iconGuide = NSLayoutGuide().customized({
    imageView.addLayoutGuide($0)
    imageView.centerYAnchor.constraint(equalTo: $0.centerYAnchor).isActive = true
  })

  public init() {
    super.init(frame: NSRect.zero)

    titleLabel.textColor = NSColor.labelColor
    titleLabel.font = .boldSystemFont(ofSize: 13)
    titleLabel.lineBreakMode = .byWordWrapping
    titleLabel.maximumNumberOfLines = 2
    titleLabel.alignment = .center

    imageView.imageScaling = .scaleProportionallyUpOrDown
    imageView.widthAnchor.constraint(equalToConstant: 156).isActive = true
    imageView.heightAnchor.constraint(equalToConstant: 96).isActive = true

    informationLabel.alignment = .center
    informationLabel.textColor = NSColor.secondaryLabelColor
    informationLabel.font = .systemFont(ofSize: 11, weight: .regular)
    informationLabel.isSelectable = false
    informationLabel.lineBreakMode = .byWordWrapping
    informationLabel.alignment = .center

    usageBar.widthAnchor.constraint(lessThanOrEqualToConstant: 130).isActive = true
    usageBar.heightAnchor.constraint(equalToConstant: 3).isActive = true

    let stackView = NSStackView(views: [titleLabel, imageView, informationLabel, usageBar])
    stackView.orientation = .vertical
    stackView.translatesAutoresizingMaskIntoConstraints = true
    stackView.autoresizingMask = [.width, .height]
    stackView.detachesHiddenViews = false
    stackView.distribution = .equalSpacing
    stackView.spacing = 0

    imageView.topAnchor.constraint(equalTo: stackView.topAnchor, constant: 47).isActive = true

    subviews = [stackView]
  }

  public override func draw(_ dirtyRect: NSRect) {
    titleLabel.textColor = NSColor.labelColor.withAlphaComponent(0.7)
    informationLabel.textColor = NSColor.labelColor.withAlphaComponent(0.7)
    super.draw(dirtyRect)
  }

  required init?(coder _: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  private let titleLabel = NSTextField(labelWithString: "")
  private let imageView = NSImageView(frame: NSRect.zero)
  private let informationLabel = NSTextField(wrappingLabelWithString: "")
  private let usageBar = SpaceUsageBar()
}
