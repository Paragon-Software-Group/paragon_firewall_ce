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
// Created by Alexey Antipov on 19.09.2019.
//

import SwiftUI

private let FastListColumnIdentifier = NSUserInterfaceItemIdentifier("elements")

struct FastList<Element, Content> : NSViewRepresentable where Element : Equatable, Element : Identifiable, Content : View {
  typealias ContentBuilder = (Element) -> Content
  
  init(_ elements: [Element], contentBuilder: @escaping ContentBuilder) {
    self.elements = elements
    self.contentBuilder = contentBuilder
  }
  
  var elements: [Element]
  
  var contentBuilder: ContentBuilder
  
  func makeCoordinator() -> FastList.Coordinator {
    return Coordinator(elements: elements, contentBuilder: contentBuilder)
  }
  
  class Coordinator: NSObject, NSTableViewDelegate, NSTableViewDataSource {
    var elements: [Element] {
      didSet {
        let changes = elements.difference(from: oldValue, by: { $0.id == $1.id })
        if !changes.isEmpty { tableView.beginUpdates() }
        for change in changes {
          switch change {
          case .insert(let offset, _, _):
            tableView.insertRows(at: IndexSet(integer: offset), withAnimation: .slideDown)
          case .remove(let offset, _, _):
            tableView.removeRows(at: IndexSet(integer: offset), withAnimation: .slideUp)
          }
        }
        if !changes.isEmpty { tableView.endUpdates() }
        let partialChanges = oldValue.applying(changes)!
        let indicesToReload = IndexSet(zip(partialChanges, elements).enumerated().compactMap { index, pair -> Int? in
          (pair.0.id == pair.1.id && pair.0 != pair.1) ? index : nil
        })
        tableView.reloadData(forRowIndexes: indicesToReload, columnIndexes: IndexSet(tableView.tableColumns.indices))
      }
    }
    
    let contentBuilder: ContentBuilder
    
    private(set) lazy var tableView: NSTableView = {
      let tableView = FastListTableView()
      tableView.dataSource = self
      tableView.delegate = self
      tableView.addTableColumn(NSTableColumn(identifier: FastListColumnIdentifier))
      tableView.usesAutomaticRowHeights = true
      tableView.headerView = nil
      tableView.intercellSpacing = NSSize(width: 0, height: 8)
      return tableView
    }()
    
    init(elements: [Element], contentBuilder: @escaping ContentBuilder) {
      self.elements = elements
      self.contentBuilder = contentBuilder
    }
    
    func numberOfRows(in tableView: NSTableView) -> Int {
      return elements.count
    }
    
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
      guard tableColumn?.identifier == FastListColumnIdentifier else { return nil }
      
      let element = elements[row]
      
      if let view = tableView.makeView(withIdentifier: FastListColumnIdentifier, owner: nil) as? NSHostingView<Content> {
        view.rootView = contentBuilder(element)
        return view
      } else {
        let rootView = contentBuilder(element)
        let view = NSHostingView(rootView: rootView)
        view.identifier = FastListColumnIdentifier
        return view
      }
    }
    
    func tableView(_ tableView: NSTableView, selectionIndexesForProposedSelection proposedSelectionIndexes: IndexSet) -> IndexSet {
      return IndexSet()
    }
  }
  
  func makeNSView(context: NSViewRepresentableContext<FastList>) -> NSScrollView {
    let scrollView = NSScrollView()
    scrollView.hasVerticalScroller = true
    scrollView.documentView = context.coordinator.tableView
    scrollView.automaticallyAdjustsContentInsets = false
    return scrollView
  }
  
  func updateNSView(_ nsView: NSScrollView, context: NSViewRepresentableContext<FastList>) {
    context.coordinator.elements = elements
  }
}

private class FastListTableView : NSTableView {
  override func validateProposedFirstResponder(_ responder: NSResponder, for event: NSEvent?) -> Bool {
    // allow mouse events for text fields
    return responder is NSTextField || super.validateProposedFirstResponder(responder, for: event)
  }
}
