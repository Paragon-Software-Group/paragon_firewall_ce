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

struct DotsView: View {
  let dotsNumber: Int

  @Binding var selectedIndex: Int
  
  var body: some View {
    HStack {
      ForEach(0..<self.dotsNumber) { idx in
        DotView(selected: Binding(get: { self.selectedIndex == idx}, set: { _ in self.selectedIndex = idx }))
      }
    }
  }
}


struct DotView : View {
  @Binding var selected: Bool
  
  let dotSize: CGFloat = 9
  let selectedDotSize: CGFloat = 14
  let colorSelected = Color(NSColor(calibratedRed: 0, green: 0x72/255, blue: 0xF8/255, alpha: 1))
  let borderColor = Color(NSColor(calibratedRed: 0x97/255, green: 0x97/255, blue: 0x97/255, alpha: 1))

  
  private var transition: AnyTransition {
    return AnyTransition.asymmetric(insertion: .scale, removal: .scale)
  }
  
  var body: some View {
    Group {
      if selected {
        Circle()
          .foregroundColor(self.colorSelected)
      }
      else {
        Circle()
          .foregroundColor(self.borderColor)
          .frame(width: self.dotSize, height: self.dotSize, alignment: .center)
      }
    }
    .transition(self.transition)
    .frame(width: self.selectedDotSize, height: self.selectedDotSize)
    .onTapGesture {
      withAnimation(.easeInOut) {
        self.selected.toggle()
      }
    }

  }
}
#if DEBUG
var dotNumber: Int = 0

struct DotsView_Previews: PreviewProvider {
//  @State static var flag: Bool = false
    static var previews: some View {
//      Circle().stroke(Color.red, lineWidth: 5)
//      DotView1(selected: true)
      DotsView(dotsNumber: 3, selectedIndex: Binding(get: {dotNumber}, set: {dotNumber = $0}))
      .frame(width: 400, height: 300)
    }
}
#endif
