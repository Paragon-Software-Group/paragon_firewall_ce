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
import nf

struct TourModeView: View {
  @Binding var mode: FilterResult
  
  var body: some View {
    HStack(alignment: .top) {
      VStack(alignment: .leading) {
        TourSectionHeader(text: "Choose between", boldText: "Alert or Silent modes")
        Text("\(AppDelegate.productNameShort) can operate in 1 of 3 modes, that you can choose from right now (and of course, change later):")
        TourModeSelectionView(selectedMode: $mode)
      }
      .frame(maxHeight: .infinity)
      Image("tour-filtermode")
    }
  }
}

#if DEBUG
struct TourDot2View_Previews: PreviewProvider {
  static var mode: FilterResult = .unknownAllow
  static var previews: some View {
    TourModeView(mode: Binding(get: {Self.mode}, set: {Self.mode = $0}))
  }
}
#endif
