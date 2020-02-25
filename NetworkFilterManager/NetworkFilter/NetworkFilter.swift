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
// Created by Alexey Antipov on 04.10.2019.
//

import Foundation
import Mach
import nf

extension RulePermission: Codable {}

public struct Application: Hashable, Codable {
  public var path: String

  public init(path: String) { self.path = path }
}

public struct Rule: Hashable, Identifiable, Codable {
  public var id: UInt64
  public var permission: RulePermission
  public var application: Application
  public var lastAccess: Date?
  public var accessCount: Int

  public init(id: UInt64, permission: RulePermission, application: Application, lastAccess: Date?, accessCount: Int) {
    self.id = id
    self.permission = permission
    self.application = application
    self.lastAccess = lastAccess
    self.accessCount = accessCount
  }
}

public struct Packet: Hashable {
  public typealias Direction = PacketDirection

  public var size: UInt32
  public var direction: Direction
  public var application: Application
  public var time: Date
}

public struct PacketList {
  public var packets: [Application: [nf_packet_info_t]]
}

public struct Statistic: Hashable {
  public var from: Date
  public var to: Date
  public var bytesIncoming: UInt64
  public var bytesOutgoing: UInt64

  public init(from: Date, to: Date, bytesIncoming: UInt64, bytesOutgoing: UInt64) {
    self.from = from
    self.to = to
    self.bytesIncoming = bytesIncoming
    self.bytesOutgoing = bytesOutgoing
  }
}

public enum RulesUpdate {
  case full([Rule])
  case partial(updated: [Rule], removed: [Rule.ID])

  public var isFull: Bool {
    if case .full = self { return true }
    else { return false }
  }
}

public extension Application {
  func withCValue<R>(_ action: (nf_application_t) throws -> R) rethrows -> R {
    try path.withCString { pathPointer in
      try action(nf_application_t(path: pathPointer))
    }
  }
}

public extension Rule {
  static func fromCValue(_ rule: nf_rule_t) -> Rule {
    Rule(
      id: rule.id,
      permission: rule.permission,
      application: Application(path: String(cString: rule.application.path)),
      lastAccess: rule.last_access != 0 ? Date(timeIntervalSince1970: TimeInterval(rule.last_access)) : nil,
      accessCount: Int(rule.access_count)
    )
  }

  func withCValue<R>(_ action: (nf_rule_t) throws -> R) rethrows -> R {
    try application.withCValue { application in
      try action(nf_rule_t(
        id: id,
        permission: permission,
        application: application,
        last_access: lastAccess.map { time_t($0.timeIntervalSince1970) } ?? 0,
        access_count: UInt64(accessCount)
      ))
    }
  }
}

public extension Packet {
  func withCValue<R>(_ action: (nf_packet_t) throws -> R) rethrows -> R {
    try application.withCValue { application in
      try action(nf_packet_t(
        size: size,
        direction: direction,
        application: application
      ))
    }
  }
}

public typealias OnlineAccessCheckCallback = (_ application: Application, _ completion: @escaping (RulePermission) -> Void) -> Void

public protocol NetworkFilterManager: AnyObject {
  typealias UpdateCallback = (_ completion: @escaping () -> Void) -> Void

  var mode: FilterResult { get set }

  var statisticEnabled: Bool { get set }

  func statistics(application: String) -> [Statistic]?

  func rules(options: RulesOptions) -> [Rule]

  func updateRule(_ rule: Rule) throws

  func removeRule(id: UInt64) throws

  func registerOnlineAccessChecker(_ callback: @escaping OnlineAccessCheckCallback)

  func registerRulesUpdateCallback(_ callback: @escaping UpdateCallback)

  func registerStatisticUpdateCallback(_ callback: @escaping UpdateCallback)
}

public extension NetworkFilterManager {
  func addRuleForApplication(at path: String) throws {
    let rule = Rule(
      id: 0,
      permission: nf_rule_permission_for_mode(mode),
      application: Application(path: path),
      lastAccess: nil,
      accessCount: 0
    )
    try updateRule(rule)
  }
}

extension Application: MachCodable {
  public func encode(with encoder: MachEncoder) {
    encoder.encodeString(path)
  }

  public init(from decoder: MachDecoder) throws {
    path = try decoder.decodeString()
  }
}

extension Date: MachCodable {
  public func encode(with encoder: MachEncoder) {
    encoder.encodeTrivial(time_t(timeIntervalSince1970))
  }

  public init(from decoder: MachDecoder) throws {
    let time: time_t = try decoder.decodeBasic()
    self.init(timeIntervalSince1970: TimeInterval(time))
  }
}

extension Rule: MachCodable {
  public func encode(with encoder: MachEncoder) {
    encoder.encodeTrivial(id)
    encoder.encodeTrivial(permission)
    application.encode(with: encoder)
    encoder.encodeOptional(lastAccess)
    encoder.encodeTrivial(accessCount)
  }

  public init(from decoder: MachDecoder) throws {
    id = try decoder.decodeBasic()
    permission = try decoder.decodeBasic()
    application = try Application(from: decoder)
    lastAccess = try decoder.decodeOptional()
    accessCount = try decoder.decodeBasic()
  }
}

extension Packet: MachDecodable {
  public init(from decoder: MachDecoder) throws {
    size = try decoder.decodeBasic()
    direction = try decoder.decodeBasic()
    application = try Application(from: decoder)
    time = try Date(from: decoder)
  }
}

extension PacketList: MachDecodable {
  public init(from decoder: MachDecoder) throws {
    let count = Int(try decoder.decodeInt32())

    packets = .init(minimumCapacity: count)

    for _ in 0 ..< count {
      let application = try Application(from: decoder)
      let infoCount = Int(try decoder.decodeInt32())
      let infoSize = infoCount * MemoryLayout<nf_packet_info_t>.stride
      let infoBase = decoder.readBytes(size: infoSize)
      let infoBufferPointer = UnsafeBufferPointer(start: infoBase.assumingMemoryBound(to: nf_packet_info_t.self), count: infoCount)
      packets[application] = .init(infoBufferPointer)
    }
  }

  var count: Int { packets.map { $0.value.count }.reduce(0, +) }
}

extension RulesUpdate: MachDecodable {
  public init(from decoder: MachDecoder) throws {
    let isFull = (try decoder.decodeInt32()) != 0

    let updated = try [Rule](from: decoder)
    let removedCount = try decoder.decodeInt32()

    var removed: [Rule.ID] = []
    for _ in 0 ..< removedCount {
      removed.append(try decoder.decodeBasic())
    }

    if isFull {
      self = .full(updated)
    } else {
      self = .partial(updated: updated, removed: removed)
    }
  }
}

public class ParagonNetworkFilterManager: NetworkFilterManager {
  let port: MachSendPort
  let server: MachServer
  let manager: nf_manager_t
  let statistics_store: nf_statistics_store_t

  var statisticServer: MachServer?

  public var statisticEnabled: Bool {
    get { statisticServer != nil }
    set {
      if !newValue {
        statisticServer?.stop()
        statisticServer = nil
        return
      }
      guard statisticServer == nil else { return }

      let server = MachServer()

      server.addCodableHandler(messageId: 202) { [unowned self] (list: PacketList, completion) in
        print("got \(list.count) packets")
        for (application, packets) in list.packets {
          for packet in packets {
            nf_statistics_store_handle_packet_info(self.statistics_store, application.path, packet)
          }
        }
        self.statCallback?(makeSafe(completion))
      }
      server.start()

      statisticServer = server

      try? Message.send(id: 252, remotePort: port, items: [.port(.makeSend(server.port))], plainData: .withUnsafeBytes(of: UInt32(0x400000)))
    }
  }

  public init(mode: FilterResult, rules: [Rule], serviceName: String) throws {
    port = try MachSendPort.lookup(name: serviceName)

    let rules = rules.map { rule -> Rule in
      var rule = rule
      rule.id = 0
      return rule
    }

    let encoder = MachEncoder()
    rules.encode(with: encoder)
    let rulesData = encoder.data

    // initialize filter
    try Message.sendWithReply(
      remotePort: port,
      messageId: 251,
      items: [.outlineData(rulesData)],
      plainData: .withUnsafeBytes(of: mode)
    ).wait().get()

    server = MachServer()
    manager = nf_manager_create()
    statistics_store = nf_statistics_store_create()

    // ask permission
    server.addCodableHandlerR(messageId: 203) { [weak self] (application: Application, replyPort) in
      self?.permissionCallback?(application) { permission in
        try? Message.send(id: 303, remote: .moveSendOnce(replyPort), plainData: .withUnsafeBytes(of: permission))
      }
    }

    // handle update
    server.addCodableHandler(messageId: 204) { [unowned self] (update: RulesUpdate, promise) in
      self.handleRulesUpdate(update, completion: makeSafe(promise))
    }

    server.start(queue: DispatchQueue(label: "com.paragon-software.FirewallApp.ipc", qos: .userInteractive))

    try Message.send(id: 200, remotePort: port, localPort: nil, items: [.port(.makeSend(server.port))], plainData: nil)
  }

  private func handleRulesUpdate(_ update: RulesUpdate, completion: @escaping () -> Void) {
    let patch = nf_rules_update_create(update.isFull)
    defer { nf_rules_update_destroy(patch) }

    switch update {
    case let .full(rules):
      for rule in rules {
        rule.withCValue { nf_rules_update_rule_updated(patch, $0) }
      }

    case let .partial(updated: rules, removed: removed):
      for rule in rules {
        rule.withCValue { nf_rules_update_rule_updated(patch, $0) }
      }
      for id in removed {
        nf_rules_update_rule_removed(patch, id)
      }
    }

    nf_manager_rules_updated(manager, patch)

    rulesUpdateCallback?(completion)
  }

  deinit {
    nf_statistics_store_destroy(statistics_store)
    nf_manager_destroy(manager)
  }

  public var mode: FilterResult {
    get {
      if let reply = try? Message.sendWithReplyRaw(remotePort: port, messageId: 202, replyLayout: MessageLayout(plainDataSize: MemoryLayout<FilterResult>.size)).wait().get() {
        return reply.plainData.load(as: FilterResult.self)
      }
      return .unknownAllow
    }
    set {
      try? Message.send(id: 201, remotePort: port, plainData: .withUnsafeBytes(of: newValue))
    }
  }

  var rulesUpdateCallback: UpdateCallback?
  var statCallback: UpdateCallback?
  var permissionCallback: OnlineAccessCheckCallback?

  public func statistics(application: String) -> [Statistic]? {
    guard let statistics = nf_statistics_store_copy_app_statistics(statistics_store, application) else { return nil }
    defer { nf_app_statistics_destroy(statistics) }

    var result: [Statistic] = []

    while let item = nf_app_statistics_next(statistics)?.pointee {
      result.append(Statistic(
        from: Date(timeIntervalSince1970: TimeInterval(item.date_from)),
        to: Date(timeIntervalSince1970: TimeInterval(item.date_to)),
        bytesIncoming: item.bytes_incoming,
        bytesOutgoing: item.bytes_outgoing
      ))
    }

    return result
  }

  public func rules(options: RulesOptions) -> [Rule] {
    var rules: [Rule] = []

    let enum_options = nf_rule_enumerator_options_t(mask: options, path: nil)
    let iterator = nf_manager_get_rules(manager, enum_options)
    defer { nf_rules_iterator_destroy(iterator) }
    while let rule = nf_rules_iterator_next(iterator)?.pointee {
      rules.append(.fromCValue(rule))
    }
    return rules
  }

  public func updateRule(_ rule: Rule) throws {
    try Message.send(id: 204, remotePort: port, items: [.codable(rule)])
  }

  public func removeRule(id: UInt64) throws {
    try Message.send(id: 205, remotePort: port, plainData: Data.withUnsafeBytes(of: id))
  }

  public func registerOnlineAccessChecker(_ callback: @escaping OnlineAccessCheckCallback) {
    permissionCallback = callback
  }

  public func registerRulesUpdateCallback(_ callback: @escaping UpdateCallback) {
    rulesUpdateCallback = callback
  }

  public func registerStatisticUpdateCallback(_ callback: @escaping UpdateCallback) {
    statCallback = callback
  }
}

private class SafeCompletionWrapper {
  typealias Completion = () -> Void

  init(_ completion: @escaping Completion) {
    completion_ = completion
  }

  deinit {
    completion_.map { $0() }
  }

  func cancel() {
    completion_ = nil
  }

  private var completion_: Completion?
}

private func makeSafe(_ completion: @escaping () -> Void) -> (() -> Void) {
  let wrapper = SafeCompletionWrapper(completion)

  return {
    wrapper.cancel()
    completion()
  }
}
