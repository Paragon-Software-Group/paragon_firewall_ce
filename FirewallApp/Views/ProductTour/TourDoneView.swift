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

struct TourDoneView: View {
  var body: some View {
    VStack {
      ZStack {
        Image("tour-app-icon")
          .resizable()
          .frame(width: 192, height: 192)
        Image("ico-ok")
          .offset(x: 192/2 - 24, y: 192/2 - 24)
      }
      
      VStack {
        Text("It's all done and ready!").font(.title)
        Text("We hope you will enjoy it!").font(.subheadline)
        
        Image("logo")
      }
      .padding([.top], 20)
    }
    .padding(40)

  }
}

#if DEBUG
struct TourDot4View_Previews: PreviewProvider {
    static var previews: some View {
        TourDoneView()
    }
}
#endif
