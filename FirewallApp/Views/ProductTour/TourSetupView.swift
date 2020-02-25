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
// Created by kaz on 21.08.2019.
//

import SwiftUI
import nf

struct TourSetupControlView: View {
  let dotsNumber: Int
  @Binding var selectedIndex: Int
  let completion: ()->Void
  
  var body: some View {
    ZStack {
      HStack {
        Spacer()
        DotsView(dotsNumber: self.dotsNumber, selectedIndex: self.$selectedIndex)
        Spacer()
      }
      HStack {
        Spacer()
        if self.selectedIndex == self.dotsNumber - 1 {
          Button(action: { self.completion() }) {
            Text("Close Tour")
              .frame(width: productTourButtonWidth)
          }
        }
        else {
          Button(action: { withAnimation { self.selectedIndex += 1 } }) {
            Text("Next")
              .frame(width: productTourButtonWidth)
          }
        }
      }.animation(nil)
    }
  }
}

struct TourSetupView: View {
  let completion: (FilterResult)->Void
  
  @State var step: Int = 0
  @State private var mode: FilterResult = .unknownAllow
  
  private var moveToLeading: AnyTransition {
    AnyTransition.move(edge: .leading)
  }
  
  var body: some View {
    VStack {
      if step == 0 {
        TourAboutView().transition(moveToLeading)
      }
      else if step == 1 {
        TourModeView(mode: self.$mode).transition(moveToLeading)
      }
      else if step == 2 {
        TourControlsView().transition(moveToLeading)
      }
      else {
        TourDoneView().transition(moveToLeading)
      }

      Spacer()

      TourSetupControlView(dotsNumber: 4, selectedIndex: self.$step, completion: {
        self.completion(self.mode)
      })
    }
  }
}

#if DEBUG
struct TourSetupView_Previews: PreviewProvider {
    static var previews: some View {
      Group {
        TourSetupView(completion: { _ in })
        TourSetupView(completion: { _ in }, step: 1)
        TourSetupView(completion: { _ in }, step: 2)
        TourSetupView(completion: { _ in }, step: 3)
      }
    }
}
#endif
