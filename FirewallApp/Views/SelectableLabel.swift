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
// Created by Alexey Antipov on 2019-10-17.
//

import SwiftUI

typealias SelectableLabel = SelectableLabel_TF

struct SelectableLabel_TF : NSViewRepresentable {
  var text: String
  
  init(_ text: String) { self.text = text }
  
  func makeNSView(context: NSViewRepresentableContext<SelectableLabel_TF>) -> NSTextField {
    let field = NSTextField(labelWithString: "")
    field.isSelectable = true
    field.lineBreakMode = .byTruncatingMiddle
    field.setContentCompressionResistancePriority(.defaultLow, for: .horizontal)
    updateNSView(field, context: context)
    return field
  }
  
  func updateNSView(_ field: NSTextField, context: NSViewRepresentableContext<SelectableLabel_TF>) {
    field.stringValue = text
    field.toolTip = text
    field.controlSize = {
      switch context.environment.controlSize {
      case .regular:
        return .regular
      case .small:
        return .small
      case .mini:
        return .mini
      @unknown default:
        return .regular
      }
    }()
  }
}

struct SelectableLabel_TV : NSViewRepresentable {
  typealias NSViewType = NSTextView
  
  var text: String
  
  init(_ text: String) {
    self.text = text
  }
  
  func makeNSView(context: NSViewRepresentableContext<SelectableLabel_TV>) -> NSViewType {
    let textView = SelectableLabelTextView()
    textView.setContentHuggingPriority(.defaultLow, for: .vertical)
    textView.translatesAutoresizingMaskIntoConstraints = false
    textView.isEditable = false
    textView.textContainer?.lineFragmentPadding = 0
    textView.textContainer?.maximumNumberOfLines = 1
    textView.textContainer?.lineBreakMode = .byTruncatingMiddle
    textView.textContainerInset = .init(width: 0, height: 0)
    textView.textContainer?.size = NSSize(width: 0, height: 17)
    textView.setConstrainedFrameSize(NSSize(width: 0, height: 17))
    updateNSView(textView, context: context)
    return textView
  }
  
  func updateNSView(_ nsView: NSViewType, context: NSViewRepresentableContext<SelectableLabel_TV>) {
    let controlSize: NSControl.ControlSize = {
      switch context.environment.controlSize {
      case .regular:
        return .regular
      case .small:
        return .small
      case .mini:
        return .mini
      @unknown default:
        return .regular
      }
    }()
    
    let string = NSAttributedString(
      string: text,
      attributes: [
        .foregroundColor : NSColor.textColor,
        .font : NSFont.systemFont(ofSize: NSFont.systemFontSize(for: controlSize))
      ]
    )
    nsView.textStorage?.setAttributedString(string)
    nsView.toolTip = text
  }
}

private class SelectableLabelTextView : NSTextView {
  override var intrinsicContentSize: NSSize { NSSize(width: -1, height: 17) }
}
