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
// Created by kaz on 30.09.2019.
//

import SwiftUI

struct AttributedText: NSViewRepresentable {
  let attributedString: NSAttributedString
  var isLabel: Bool = true
  var isSelectable: Bool = true
  
  func makeNSView(context: Self.Context) -> NSScrollView {
    let textView = NSTextView()
    textView.textStorage?.setAttributedString(context.coordinator.attributedString)
    textView.isEditable = false
    textView.isSelectable = isSelectable
    textView.drawsBackground = !isLabel
    textView.autoresizingMask = [.width]
    if isLabel {
      textView.textContainer?.lineFragmentPadding = 0
    }
    let scrollView = NSScrollView()
    scrollView.hasVerticalScroller = !isLabel
    scrollView.borderType = isLabel ? .noBorder : .lineBorder
    scrollView.documentView = textView
    scrollView.drawsBackground = !isLabel
    return scrollView
  }

  /// Updates the presented `NSView` (and coordinator) to the latest
  /// configuration.
  func updateNSView(_ nsView: NSScrollView, context: Self.Context) {
    guard let textView = nsView.documentView as? NSTextView else { return }
    textView.textStorage?.setAttributedString(context.coordinator.attributedString)
  }

  func makeCoordinator() -> Self.Coordinator {
    return Coordinator(string: attributedString)
  }
  
  class Coordinator {
    var attributedString: NSAttributedString
    
    init(string: NSAttributedString) {
      attributedString = string
    }
  }
}

struct AttributedText_Previews: PreviewProvider {
  static let rtf: NSAttributedString = {
    let data = NSDataAsset(name: "rtf")
    return NSAttributedString(rtf: data!.data, documentAttributes: nil)!
  }()
  static var previews: some View {
    Group {
      AttributedText(attributedString: rtf, isLabel: false, isSelectable: true)
      AttributedText(attributedString: NSAttributedString(string: "This is attributed string"))
    }
  }
}
