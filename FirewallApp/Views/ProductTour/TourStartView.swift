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

struct TourStartView: View {
  let completion: ()->Void
  
  var body: some View {
    VStack {
      VStack {
        Image("tour-app-icon")
          .resizable()
          .frame(width: 192, height: 192)

        Text("Welcome to").font(.subheadline)
        Text(AppDelegate.productNameFull).font(.title)
      }
      .padding(.top, 40)
      
      Spacer()
      
      HStack(alignment: .center, spacing: 3) {
        Text("Read our Privacy Policy")
        LinkButton("here") {
          guard let url = URL(string: AppDelegate.productPolicyAddress) else { return }
          NSWorkspace.shared.open(url)
        }
        Text("and Terms of Use")
        LinkButton("here") {
          guard let url = URL(string: AppDelegate.productTermsOfUseAddress) else { return }
          NSWorkspace.shared.open(url)
        }
      }
      .font(.footnote)


      Spacer()

      ZStack(alignment: .bottom) {
        Image("logo")
        HStack {
          Spacer()
          Button(action: { self.completion() }) {
            Text("Start")
              .frame(width: productTourButtonWidth)
          }
        }
      }
    }
  }
}

#if DEBUG
struct TourStartView_Previews: PreviewProvider {
    static var previews: some View {
      TourStartView(completion: {})
    }
}
#endif
