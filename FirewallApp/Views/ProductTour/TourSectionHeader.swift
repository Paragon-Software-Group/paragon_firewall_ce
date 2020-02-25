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
// Created by kaz on 26.08.2019.
//

import SwiftUI

struct TourSectionHeader: View {
  let text: String
  let boldText: String
  
  var body: some View {
    VStack(alignment: .leading) {
      Text(self.text).font(Font.system(size: 18, weight: .light, design: .default))
      Text(self.boldText).font(Font.system(size: 21, weight: .bold, design: .default))
    }
    .padding([.bottom], 15)
  }
}

#if DEBUG
struct TourSectionHeader_Previews: PreviewProvider {
    static var previews: some View {
      TourSectionHeader(text: "Add apps with connections", boldText: "you need to block")
    }
}
#endif
