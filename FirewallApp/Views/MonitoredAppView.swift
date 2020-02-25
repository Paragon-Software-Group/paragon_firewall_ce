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
// Created by kaz on 22.07.2019.
//

import Foundation
import SwiftUI
import NetworkFilter
import nf

struct MonitoredAppView : View {
  var appModel: MonitoredAppModel
  let removeCommand: () -> Void
  let replaceRuleCommand: (Rule) -> Void
  
  @State private var showStatistic = false
  private let iconSize: CGFloat = 48
  
  var body: some View {
    VStack {
      HStack {
        if appModel.statistic != nil {
          Button(action: {
            withAnimation {
              self.showStatistic.toggle()
            }
          }) {
            Text("▶")
              .rotationEffect(Angle(degrees: showStatistic ? 90 : 0))
              .animation(.easeInOut)
          }
          .disabled(self.appModel.statistic?.isEmpty == true)
          .buttonStyle(BorderlessButtonStyle())
          .frame(width: 12)
        }
        
        HStack {
          IconView(image: appModel.icon, size: NSSize(width: iconSize, height: iconSize))
            .frame(width: iconSize, height: iconSize)
          
          VStack(alignment: .leading, spacing: 3) {
            Text(appModel.name)
              .fontWeight(.bold)
              .font(Font.system(size: 13))
            
            SelectableLabel(appModel.applicationInfo.displayPath)
              .controlSize(.small)
            MonitoredAppBriefStats(
              lastAccess: appModel.lastAccess,
              traffic: appModel.traffic)
          }
        }
        
        Spacer()
        
        Picker(selection: Binding<RulePermission>(get: { self.appModel.permission }, set: setPermission(_:)), label: Text("")) {
          HStack {
            Image("allow-forever")
            Text("Allow")
          }.tag(RulePermission.allow)
          HStack {
            Image("block-forever")
            Text("Block")
          }.tag(RulePermission.deny)
        }
        .pickerStyle(PopUpButtonPickerStyle())
        .frame(width:120)
        
        Button(action: removeCommand) {
          Image("delete-rule")
        }
        .buttonStyle(BorderlessButtonStyle())
      }.animation(nil)
      
      if showStatistic {
        ApplicationStatisticsView(statistics: appModel.statistic ?? [])
          .frame(minHeight: 127)
          .shadow(radius: 1, x: 0, y: 1)
          .transition(AnyTransition.move(edge: .top)
            .animation(.easeInOut).combined(with: .opacity))
      } else {
        Divider()
      }
    }
  }
  
  private func setPermission(_ permission: RulePermission) {
    var rule = appModel.rule
    rule.permission = permission
    replaceRuleCommand(rule)
  }
}

#if DEBUG
struct MonitoredAppView_Previews : PreviewProvider {
  static let now = Date()
  static let application = Application(path: "/Library/Developer/PrivateFrameworks/CoreSimulator.framework/Versions/A/XPCServices/com.apple.CoreSimulator.CoreSimulatorService.xpc/Contents/MacOS/com.apple.CoreSimulator.CoreSimulatorService")
  static let rule = Rule(id: 1, permission: .allow, application: application, lastAccess: now, accessCount: 1)
  static let statistic = Statistic(from: now.advanced(by: -3600), to: now, bytesIncoming: 0x1234, bytesOutgoing: 0x4567)
  static let appModel = MonitoredAppModel(rule: rule, statistic: [statistic])
  static let previews = MonitoredAppView(appModel: appModel, removeCommand: {}, replaceRuleCommand: { _ in })
}
#endif
