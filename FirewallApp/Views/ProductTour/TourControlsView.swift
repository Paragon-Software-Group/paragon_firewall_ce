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
// Created by kaz on 19.08.2019.
//

import SwiftUI

struct TourControlsView: View {
  let description: NSAttributedString = {
    let attrstr = NSMutableAttributedString()
    let attachment = NSTextAttachment()
    attachment.image = #imageLiteral(resourceName: "tour-plus-button")
    attrstr.append(NSAttributedString(string: "Hit the "))
    attrstr.append(NSAttributedString(attachment: attachment))
    attrstr.append(NSAttributedString(string: " button to manually add applications whose connections you need to block. Later on, you can always change your decision."))
    attrstr.addAttribute(.font, value: NSFont.systemFont(ofSize: 13), range: NSRange(location: 0, length: attrstr.length))
    attrstr.addAttribute(.foregroundColor, value: NSColor.labelColor, range: NSRange(location: 0, length: attrstr.length))
    return attrstr
  }()
  
  var body: some View {
    GeometryReader { geometry in
      HStack(alignment: .top) {
        VStack {
          VStack(alignment: .leading){
            TourSectionHeader(text: "Add apps with connections", boldText: "you need to block")
            AttributedText(attributedString: self.description, isLabel: true, isSelectable: false)
          }
          .frame(height: geometry.size.height / 2.5)
          Image("tour-toolbar-left").offset(y: -12)
        }
        VStack(alignment: .leading) {
          Image("tour-toolbar-right").offset(x: -12, y: -12)
          VStack(alignment: .leading) {
            TourSectionHeader(text: "Use search and filters for", boldText: "easy app navigation")
            Text("Using filters, sorting and search, you can easily navigate between apps and adjust your Network Monitor view to fit your needs.")
              .frame(height: 70, alignment: .top)
          }
        }
      }
    }
  }
}

#if DEBUG
struct TourDot3View_Previews: PreviewProvider {
    static var previews: some View {
        TourControlsView()
    }
}
#endif
