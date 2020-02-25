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
// Created by kaz on 23.08.2019.
//

import SwiftUI
import NetworkFilter
import nf

struct TourModeAllowNewView: View {
  var body: some View {
    VStack(alignment: .leading) {
      HStack {
        Image(nsImage: #imageLiteral(resourceName: "ico-silent-new-allowed"))
        Text("Silent mode - All new apps are allowed")
          .opacity(0.75)
      }
      Group {
        Text("Recommended for new users.").bold() +
        Text(" ") +
        Text("Alerts won’t bother you, all new app connections are allowed unless you explicitly block them.")
      }
      .font(.system(size: 11))
      .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }
  }
}

struct TourModeBlockNewView: View {
  var body: some View {
    VStack(alignment: .leading) {
      HStack {
        Image(nsImage: #imageLiteral(resourceName: "ico-silent-new-blocked"))
        Text("Silent mode - All new apps are blocked")
          .opacity(0.75)
      }
      Text("Alerts still won’t bother you; all new apps are blocked unless you manually allow them.")
        .font(.system(size: 11))
      .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }
  }
}

struct TourModeAlertView: View {
  var body: some View {
    VStack(alignment: .leading) {
      HStack {
        Image(nsImage: #imageLiteral(resourceName: "ico-alerts-on"))
        Text("Alert mode - Get alerted on all new apps")
          .opacity(0.75)
      }
      Text("For advanced users who want to take immediate actions - Block or allow each new app that tries to establish an incoming or outgoing connection.")
        .font(.system(size: 11))
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
    }
  }
}

struct TourModeSelectionView: View {
  let borderColor = Color(Color.RGBColorSpace.sRGB, red: 0, green: 0x84/255, blue: 0xFF/255, opacity: 1)
  let fillColor = Color(.sRGB, red: 0, green: 132/255, blue: 1, opacity: 0.25)
  
  @Binding var selectedMode: FilterResult
  
  var body: some View {
    GeometryReader { geometry in
      VStack(alignment: .leading) {
        TourModeAllowNewView()
        .padding(5)
        .overlay(
          RoundedRectangle(cornerRadius: 3)
            .fill(self.selectedMode == .unknownAllow ? self.fillColor : Color.clear)
            .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(self.selectedMode == .unknownAllow ? self.borderColor : Color.clear, lineWidth: 1)))
          .frame(width: geometry.size.width)
          .onTapGesture {
            withAnimation {
              self.selectedMode = .unknownAllow
            }
          }
        
        TourModeBlockNewView()
        .padding(5)
        .overlay(
          RoundedRectangle(cornerRadius: 3)
            .fill(self.selectedMode == .unknownDeny ? self.fillColor : Color.clear)
            .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(self.selectedMode == .unknownDeny ? self.borderColor : Color.clear, lineWidth: 1)))
          .frame(width: geometry.size.width)
          .onTapGesture {
            withAnimation {
              self.selectedMode = .unknownDeny
            }
          }

        TourModeAlertView()
        .padding(5)
        .overlay(
          RoundedRectangle(cornerRadius: 3)
            .fill(self.selectedMode == .wait ? self.fillColor : Color.clear)
            .overlay(RoundedRectangle(cornerRadius: 3).strokeBorder(self.selectedMode == .wait ? self.borderColor : Color.clear, lineWidth: 1)))
          .frame(width: geometry.size.width)
          .onTapGesture {
            withAnimation {
              self.selectedMode = .wait
            }
        }
      }
    .lineLimit(15)
    }
  }
}

#if DEBUG
struct TourModeSelectionView_Previews: PreviewProvider {
    static var previews: some View {
      Group {
        TourModeSelectionView(selectedMode: Binding<FilterResult>(get: { return .unknownAllow }, set: {_ in}))

        TourModeBlockNewView()
        TourModeAllowNewView()
        TourModeAlertView()
      }
      .frame(width: 300)
    }
}
#endif
