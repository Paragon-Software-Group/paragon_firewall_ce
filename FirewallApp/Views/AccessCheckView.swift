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
// Created by kaz on 07.08.2019.
//

import SwiftUI
import NetworkFilter
import nf

struct AccessCheckView: View {
  let applicationInfo: ApplicationInfo
  
  let completion: (RulePermission) -> Void
  
  var body: some View {
    VStack(alignment: .leading) {
      HStack(alignment: .top, spacing: 17.5) {
        Image(nsImage: applicationInfo.icon)
          .resizable()
          .frame(width: 60.5, height: 60.5)
        
        VStack(alignment: .leading, spacing: 10) {
          Text(applicationInfo.name)
            .font(.system(size: 18, weight: .bold))
          
          Text("Do you want to allow or block connection?")
            .lineLimit(nil)
          
          Text(applicationInfo.displayPath)
            .font(.system(size: 11))
            .opacity(0.5)
        }
      }
      Spacer()
      HStack {
        Spacer()
        Button(action: {
          self.completion(.deny)
        }) {
          Text("Block").frame(minWidth: 76)
        }
        Button(action: {
          self.completion(.allow)
        }) {
          Text("Allow").frame(minWidth: 76)
        }
      }
    }
    .padding()
  }
}

extension Bundle {
  var icon: NSImage? {
    (infoDictionary?["CFBundleIconFile"] as? String).flatMap { image(forResource: NSImage.Name($0)) }
  }
}

#if DEBUG
struct AccessCheckView_Previews: PreviewProvider {
  static var previews: some View {
    AccessCheckView(
      applicationInfo: ApplicationInfo(
        name: "Skype",
        icon: NSWorkspace.shared.icon(forFile: "/Applications/Skype.app"),
        displayPath: "/Applications/Skype.app"),
      completion: { _ in }
    )
  }
}
#endif
