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

struct TourAboutView: View {
  var body: some View {
    HStack(alignment: .top) {
      VStack(alignment: .leading) {
        TourSectionHeader(text: "All app internet connections are", boldText: "now under your control")
        Text("If any app tries to establish an outgoing or incoming connection, with \(AppDelegate.productNameShortest), you will be aware of it.")
          .padding([.vertical], 10)
        Text("It's up to you to decide whether to allow or block application connections and for how long.")
        Spacer()
      }
      Image("tour-screenshot")
        .offset(y: -12)
    }
    .padding(.vertical, 40)
  }
}

#if DEBUG
struct TourDot1View_Previews: PreviewProvider {
    static var previews: some View {
        TourAboutView()
    }
}
#endif
