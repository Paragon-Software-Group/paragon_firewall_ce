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
// Created by kaz on 24.10.2019.
//

import SwiftUI

struct IconView : NSViewRepresentable {
  let image: NSImage
  
  init(image: NSImage, size: NSSize) {
    self.image = image.copy() as! NSImage
    self.image.resizingMode = .stretch
    self.image.size = size
  }
  
  func makeNSView(context: Self.Context) -> NSView {
    let view = NSView()
    view.autoresizingMask = [.height, .width]
    view.wantsLayer = true
    updateNSView(view, context: context)
    return view
  }

  /// Updates the presented `NSView` (and coordinator) to the latest
  /// configuration.
  func updateNSView(_ nsView: NSView, context: Self.Context) {
    nsView.layer?.contents = image.layerContents(forContentsScale: context.environment.displayScale)
  }
}
