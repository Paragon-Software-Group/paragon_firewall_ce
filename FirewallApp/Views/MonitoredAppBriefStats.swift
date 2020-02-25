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
// Created by kaz on 02.08.2019.
//

import SwiftUI
//import NetworkFilterManager

struct MonitoredAppBriefStats: View {
  let lastAccess: Date?
  let traffic: Int64?
  
  private var lastAccessString: String {
    get {
      guard let lastAccess = lastAccess else { return "never" }
      let lastAccessComponents = Calendar.current.dateComponents([.hour, .minute], from: lastAccess, to: Date())
      if let hours = lastAccessComponents.hour, hours != 0 {
        return DateFormatter.localizedString(from: lastAccess, dateStyle: .short, timeStyle: .short)
      }
      if let minutes = lastAccessComponents.minute, minutes != 0 {
        return "\(minutes) minute(s) ago"
      }
      return "moments ago"
    }
  }
  
  var body: some View {
    HStack {
      if traffic != nil {
        Text("Traffic: \(ByteCountFormatter.string(fromByteCount: traffic!, countStyle: .decimal))")
        Text("|")
      }
      Text("Last access: \(lastAccessString)")
    }.font(Font.system(size: 11).weight(.light))
  }
}

#if DEBUG
struct MonitoredAppBriefStats_Previews : PreviewProvider {
  static var previews: some View {
    Group {
      MonitoredAppBriefStats(lastAccess: nil, traffic: 0)
      MonitoredAppBriefStats(lastAccess: Date(), traffic: 5)
      MonitoredAppBriefStats(lastAccess: Date(timeInterval: -600, since: Date()), traffic: 1000)
      MonitoredAppBriefStats(lastAccess: Date(timeInterval: -4000, since: Date()), traffic: 1024)
    }
  }
}
#endif
