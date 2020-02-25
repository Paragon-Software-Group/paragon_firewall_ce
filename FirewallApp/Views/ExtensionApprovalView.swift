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

struct ExtensionApprovalView: View {
    var body: some View {
      VStack(alignment: .leading) {
        Text("Allow software from").font(.subheadline)
        Text("Paragon Software GmbH").font(.title)

        VStack(alignment: .center) {
          Image("tour-security-allow")
          
          Text("Please allow Firewall System Extension by Paragon Software GmbH in System Preferences.")
            .padding(.vertical)
          
          Spacer()

          Image("logo")
          
          Spacer()

          Button(action: {
            guard let url = URL(string: "x-apple.systempreferences:com.apple.preference.security") else {
              fatalError()
            }
            NSWorkspace.shared.open(url)
          }) {
            Text("Open Security & Privacy Preferences...")
          }
        }
      }
    }
}

#if DEBUG
struct ExtensionApprovalView_Previews: PreviewProvider {
    static var previews: some View {
      ExtensionApprovalView()
    }
}
#endif
