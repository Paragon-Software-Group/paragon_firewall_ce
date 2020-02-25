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
import NetworkFilter
import nf

let productTourButtonWidth: CGFloat = 93

struct TourView: View {
  enum Stage : Equatable{
    case start
    case setup
  }
  
  @State var stage: Stage = .start

  let completion: (FilterResult)->Void

  var body: some View {
    VStack {
      if self.stage == .start {
        TourStartView(completion: { self.stage = .setup })
      }
      else {
        TourSetupView(completion: { self.completion($0) })
      }
    }
    .padding()
  }
}

#if DEBUG
struct TourView_Previews: PreviewProvider {
    static var previews: some View {
      Group {
        TourView(completion: { _ in })
        TourView(stage: .start, completion: { _ in })
        TourView(stage: .setup, completion: { _ in })
      }
    }
}
#endif
