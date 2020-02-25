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
// Created by kaz on 29.07.2019.
//

import SwiftUI
import Combine
import NetworkFilter

struct ApplicationStatisticsView: NSViewRepresentable {
  var statistics: [Statistic]
  
  class Coordinator : NSObject, NSTableViewDataSource, NSTableViewDelegate {
    struct StatisticData {
      var bytesIn: UInt64
      var bytesOut: UInt64
    }

    private enum Column: String, CaseIterable {
      case hour, bytesIn, bytesOut
      
      var nsColumn: NSTableColumn {
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier(self.rawValue))
        switch self {
        case .hour:
          column.title = NSLocalizedString("Date", comment: "")

        case .bytesIn:
          column.title = NSLocalizedString("Incoming", comment: "")
          
        case .bytesOut:
          column.title = NSLocalizedString("Outgoing", comment: "")
        }
        return column
      }
    }
    
    lazy var tableView: NSTableView = {
      let tableView = NSTableView(frame: .zero)
      tableView.delegate = self
      tableView.dataSource = self
      tableView.columnAutoresizingStyle = .uniformColumnAutoresizingStyle
      tableView.selectionHighlightStyle = .none
      tableView.allowsColumnReordering = false
      Column.allCases.forEach { column in
        tableView.addTableColumn(column.nsColumn)
      }
      return tableView
    }()
    
    var statistics: [Statistic] {
      didSet {
        tableView.reloadData()
      }
    }

    init(statistics: [Statistic]) {
      self.statistics = statistics
    }

    //MARK:- NSTableViewDelegate
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
      guard let identifier = tableColumn?.identifier, let column = Column(rawValue: identifier.rawValue) else { return nil }
      
      let text: String
      
      switch column {
      case .hour:
        text = DateFormatter.localizedString(from: statistics[row].from, dateStyle: .short, timeStyle: .medium)
        
      case .bytesIn:
        text = ByteCountFormatter.string(fromByteCount: Int64(statistics[row].bytesIncoming), countStyle: .memory)
        
      case .bytesOut:
        text = ByteCountFormatter.string(fromByteCount: Int64(statistics[row].bytesOutgoing), countStyle: .memory)
      }
      
      if let textField = tableView.makeView(withIdentifier: identifier, owner: nil) as? NSTextField {
        textField.stringValue = text
        return textField
      } else {
        let textField = NSTextField(labelWithString: text)
        textField.font = NSFont.systemFont(ofSize: 11, weight: .thin)
        textField.identifier = identifier
        return textField
      }
    }
      
    //MARK:- NSTableViewDataSource
    func numberOfRows(in tableView: NSTableView) -> Int {
      return statistics.count
    }
  }

  init(statistics: [Statistic]) {
    self.statistics = statistics.filter { $0.bytesIncoming + $0.bytesOutgoing != 0 }.sorted { $0.from > $1.from }
  }
  
  func makeCoordinator() -> Coordinator {
    return Coordinator(statistics: statistics)
  }
  
  func makeNSView(context: NSViewRepresentableContext<ApplicationStatisticsView>) -> NSView {
    let scrollView = NSScrollView()
    scrollView.hasVerticalScroller = true
    scrollView.documentView = context.coordinator.tableView
    return scrollView
  }

  func updateNSView(_ nsView: NSView, context: NSViewRepresentableContext<ApplicationStatisticsView>) {
    context.coordinator.statistics = statistics
  }
}

extension Date {
  static func getHourStartDate(for date: Date) -> Date {
    let hourStartInterval = (date.timeIntervalSinceReferenceDate / 3600).rounded(.down) * 3600
    return Date(timeIntervalSinceReferenceDate: hourStartInterval)
  }
}

#if DEBUG
struct ApplicationStatisticsTableView_Previews: PreviewProvider {
  static let statistics: [Statistic] = (1...32).map { (i: Int) -> Statistic in
    Statistic(from: Date(timeIntervalSinceNow: Double(i) + 1),
              to: Date(timeIntervalSinceNow: Double(i) + 2),
              bytesIncoming: UInt64(i) * 4,
              bytesOutgoing: UInt64(i) * 16)
  }
  
  static let previews = ApplicationStatisticsView(statistics: statistics)
}
#endif
