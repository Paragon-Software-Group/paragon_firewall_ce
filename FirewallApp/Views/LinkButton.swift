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
// Created by kaz on 22.10.2019.
//

import SwiftUI

struct LinkButton : View {
  var title: String
  var action: () -> Void
  
  init(_ title: String, action: @escaping () -> Void) {
    self.title = title
    self.action = action
  }
  
  init(_ title: String, _ url: URL) {
    self.init(title) {
      NSWorkspace.shared.open(url)
    }
  }
  
  var body: some View {
    Text(title)
      .foregroundColor(Color(NSColor.linkColor))
      .underline(isHovered, color: nil)
      .onHover(perform: { hovered in
        self.isHovered = hovered
        (hovered ? NSCursor.pointingHand : NSCursor.arrow).set()
      })
      .onTapGesture(perform: action)
  }
  
  @State private var isHovered: Bool = false
}

struct LinkButton_Previews: PreviewProvider {
    static var previews: some View {
      LinkButton("google", URL(string: "https://google.com")!)
    }
}
