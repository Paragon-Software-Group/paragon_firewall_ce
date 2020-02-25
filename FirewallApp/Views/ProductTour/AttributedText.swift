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

struct AttributedText2: NSViewRepresentable {
  let attributedString: NSAttributedString
  let drawBackground: Bool = false
  let textPadding: CGFloat? = nil

  func makeNSView(context: NSViewRepresentableContext<AttributedText2>) -> NSTextView {
    let textView = NSTextView()
    textView.textStorage?.setAttributedString(context.coordinator.attributedString)
    textView.isEditable = false
    textView.drawsBackground = drawBackground
    textPadding.map { textView.textContainer?.lineFragmentPadding = $0 }
    return textView
  }

  /// Updates the presented `NSView` (and coordinator) to the latest
  /// configuration.
  func updateNSView(_ nsView: NSTextView, context: Self.Context) {
    nsView.textStorage?.setAttributedString(context.coordinator.attributedString)
  }

  func makeCoordinator() -> Self.Coordinator {
    return Coordinator(string: attributedString)
  }
  
  class Coordinator {
    let attributedString: NSAttributedString
    
    init(string: NSAttributedString) {
      attributedString = string
    }
  }
}


struct AttributedText_Previews: PreviewProvider {
  static var attributedString: NSAttributedString = {
    let attrstr = NSMutableAttributedString(string: "Hit button", attributes: [.font : NSFont.systemFont(ofSize: 5)])
    let attachment = NSTextAttachment()
    let image = NSImage(named: NSImage.addTemplateName)
    attachment.image = image
    attrstr.append(NSAttributedString(attachment: attachment))
    attrstr.append(NSAttributedString(string: "to add an app"))
    return attrstr
  }()
    static var previews: some View {
      AttributedText2(attributedString: Self.attributedString)
    }
}
