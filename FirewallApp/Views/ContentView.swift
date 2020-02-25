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
// Created by kaz on 11.07.2019.
//

import SwiftUI

struct ContentView : View {
    @EnvironmentObject var filterModel: NetFilterModel
  
  var filteringRules: Bool {
    filterModel.rulesQueryApplicationMask != nil || filterModel.rulesOptions != .showAll
  }
    
    var body: some View {
      Group {
        if filteringRules && filterModel.appsInfo.isEmpty {
          Text("No entries were found")
            .foregroundColor(Color(NSColor.secondaryLabelColor))
        }
        else{
          FastList(filterModel.appsInfo) { app in
            MonitoredAppView(appModel: app, removeCommand: {
              self.filterModel.deleteRule(withIdentifier: app.rule.id)
            }, replaceRuleCommand: { rule in
              self.filterModel.replaceRule(rule)
            })
              .padding(.horizontal)
          }
        }
      }
      .frame(minWidth: 400, maxWidth: .infinity, minHeight: 60, maxHeight: .infinity)
    }
}

#if DEBUG
struct ContentView_Previews : PreviewProvider {
    static var previews: some View {
        //ContentView(model: NetFilterModel(NetworkFilterManager: NetworkFilterManager()))
        
//        MonitoredAppCell(model: MonitoredAppModel(path: URL(fileURLWithPath: "/Applications/Safari.app", isDirectory: true)))
        EmptyView()
    }
}
#endif
