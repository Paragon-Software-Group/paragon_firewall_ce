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
// Created by Alexey Antipov on 15/02/2019.
//

import Core
import Foundation

public enum ReceiveError: LocalizedError {
  case invalidReplyMessageId
  case invalidReply
  case serverError(code: Int)
  case machError(code: Int)

  public var errorDescription: String? {
    let code: Int

    switch self {
    case .invalidReplyMessageId:
      code = Int(MIG_REPLY_MISMATCH)

    case .invalidReply:
      code = Int(MIG_TYPE_ERROR)

    case .serverError:
      return String(describing: self)

    case let .machError(machCode):
      code = machCode
    }

    return NSError(domain: MachError.errorDomain, code: code, userInfo: nil).localizedDescription
  }
}

public enum SendError: Error {
  case machError(code: Int)
}

public struct MessageLayout: Equatable {
  public enum Item: Equatable {
    case port
    case outlineData

    public var size: Int {
      switch self {
      case .port:
        return MemoryLayout<mach_msg_port_descriptor_t>.size

      case .outlineData:
        return MemoryLayout<mach_msg_ool_descriptor_t>.size
      }
    }
  }

  public let items: [Item]

  public let plainDataSize: Int

  public var bodyOffset: Int = MemoryLayout<mach_msg_header_t>.size

  public var plainDataOffset: Int {
    if items.isEmpty { return bodyOffset }

    return bodyOffset + MemoryLayout<mach_msg_body_t>.size + items.map({ $0.size }).reduce(0, +)
  }

  public var size: Int {
    return plainDataOffset + plainDataSize
  }

  public var isComplex: Bool { return !items.isEmpty }

  var errorLayout: MessageLayout {
    let intSize = 4

    if isComplex || plainDataSize != intSize {
      return MessageLayout(plainDataSize: intSize)
    } else {
      return MessageLayout(plainDataSize: intSize * 2)
    }
  }

  public init(items: [Item] = [], plainDataSize: Int = 0) {
    self.items = items
    self.plainDataSize = plainDataSize
  }

  init(_ headerPointer: UnsafePointer<mach_msg_header_t>) {
    let header = headerPointer.pointee
    let messageSize = Int(header.msgh_size)

    if (header.msgh_bits & MACH_MSGH_BITS_COMPLEX) != 0 {
      let (plainDataOffset, items) = MessageLayout.decodeItems(headerPointer: headerPointer)

      self.items = items
      self.plainDataSize = messageSize - plainDataOffset
    } else {
      items = []
      plainDataSize = messageSize - MemoryLayout<mach_msg_header_t>.size
    }
  }

  func offsetOfItem(at index: Int) -> Int {
    return bodyOffset + MemoryLayout<mach_msg_body_t>.size + items[0 ..< index].map({ $0.size }).reduce(0, +)
  }

  private static func decodeItems(headerPointer: UnsafePointer<mach_msg_header_t>) -> (plainDataOffset: Int, items: [Item]) {
    let bodyPointer = UnsafeRawPointer(headerPointer)
      .advanced(by: MemoryLayout<mach_msg_header_t>.size)
      .assumingMemoryBound(to: mach_msg_body_t.self)

    var itemRawPointer = UnsafeRawPointer(bodyPointer)
      .advanced(by: MemoryLayout<mach_msg_body_t>.size)

    let items: [Item] = (0 ..< bodyPointer.pointee.msgh_descriptor_count).map { _ in
      let typePointer = itemRawPointer.assumingMemoryBound(to: mach_msg_type_descriptor_t.self)
      let item: Item

      switch Int32(typePointer.pointee.type) {
      case MACH_MSG_OOL_DESCRIPTOR:
        item = .outlineData

      case MACH_MSG_PORT_DESCRIPTOR:
        item = .port

      default:
        fatalError()
      }

      itemRawPointer = itemRawPointer.advanced(by: item.size)

      return item
    }

    let plainDataOffset = itemRawPointer - UnsafeRawPointer(headerPointer)

    return (plainDataOffset, items)
  }
}

public class Message {
  public let layout: MessageLayout

  public var header: mach_msg_header_t {
    return withBufferData { $0.assumingMemoryBound(to: mach_msg_header_t.self).pointee }
  }

  public var plainData: UnsafeRawBufferPointer {
    return withBufferData { pointer in
      let base = pointer.advanced(by: layout.plainDataOffset)
      return UnsafeRawBufferPointer(start: base, count: layout.plainDataSize)
    }
  }

  public var auditToken: audit_token_t? {
    return withBufferData { pointer -> audit_token_t? in
      let trailerBase = pointer.advanced(by: layout.size)
      let trailer = trailerBase.assumingMemoryBound(to: mach_msg_trailer_t.self).pointee

      guard trailer.msgh_trailer_size >= MemoryLayout<mach_msg_audit_trailer_t>.size else { return nil }

      return trailerBase
        .assumingMemoryBound(to: mach_msg_audit_trailer_t.self)
        .pointee
        .msgh_audit
    }
  }

  public func extractOutlineData(at index: Int) -> Data {
    assert(index < layout.items.count)
    assert(layout.items[index] == .outlineData)

    let offset = layout.offsetOfItem(at: index)

    return withBufferData { pointer in
      let descriptorPointer = pointer.advanced(by: offset).assumingMemoryBound(to: mach_msg_ool_descriptor_t.self)
      let descriptor = descriptorPointer.pointee

      guard let address = descriptor.address else { return Data() }

      if descriptor.deallocate != 0 {
        let data = Data(
          bytesNoCopy: address,
          count: Int(descriptor.size),
          deallocator: .virtualMemory
        )

        descriptorPointer.pointee.address = nil
        descriptorPointer.pointee.size = 0
        descriptorPointer.pointee.deallocate = 0

        return data
      } else {
        return Data(bytes: address, count: Int(descriptor.size))
      }
    }
  }

  public func extractSendRight(at index: Int) throws -> MachSendPort {
    assert(index < layout.items.count)
    assert(layout.items[index] == .port)

    let offset = layout.offsetOfItem(at: index)

    return try withBufferData { pointer in
      let descriptorPointer = pointer
        .advanced(by: offset)
        .assumingMemoryBound(to: mach_msg_port_descriptor_t.self)

      return try MachSendPort(&descriptorPointer.pointee)
    }
  }

  public func extractSendOnceRight(at index: Int) throws -> MachSendOncePort {
    assert(index < layout.items.count)
    assert(layout.items[index] == .port)

    let offset = layout.offsetOfItem(at: index)

    return try withBufferData { pointer in
      let descriptorPointer = pointer
        .advanced(by: offset)
        .assumingMemoryBound(to: mach_msg_port_descriptor_t.self)

      return try MachSendOncePort(&descriptorPointer.pointee)
    }
  }

  public func extractReplySendOnceRight() -> MachSendOncePort? {
    let remote_name = header.msgh_remote_port

    guard Int32(header.msgh_bits & UInt32(MACH_MSGH_BITS_REMOTE_MASK)) == MACH_MSG_TYPE_MOVE_SEND_ONCE,
      remote_name != 0
    else { return nil }

    withBufferData {
      $0.assumingMemoryBound(to: mach_msg_header_t.self)
        .pointee
        .msgh_remote_port = 0
    }

    return MachSendOncePort.construct(name: remote_name)
  }

  public static func receive(maxSize: Int, port: mach_port_t) throws -> Message {
    let bufferSize = maxSize + MemoryLayout<mach_msg_audit_trailer_t>.size

    var buffer = Data(capacity: bufferSize)

    let layout = try buffer.withUnsafeMutableBytes { pointer -> MessageLayout in
      let rawPointer = pointer.baseAddress!
      let headerPointer = rawPointer.assumingMemoryBound(to: mach_msg_header_t.self)
      let options: mach_msg_option_t = MACH_RCV_MSG | (MACH_RCV_TRAILER_AUDIT << 24)

      let status = mach_msg(headerPointer, options, 0, mach_msg_size_t(bufferSize), port, 0, 0)

      guard status == KERN_SUCCESS else {
        throw ReceiveError.machError(code: Int(status))
      }

      return MessageLayout(headerPointer)
    }

    return Message(buffer: buffer, layout: layout)
  }

  public static func receive(layout: MessageLayout, id: mach_msg_id_t, port: mach_port_t) throws -> Message {
    let message = try receive(maxSize: layout.size, port: port)

    guard message.header.msgh_id == id else {
      throw ReceiveError.invalidReplyMessageId
    }

    guard message.layout == layout else {
      if message.layout == layout.errorLayout {
        let code = message.plainData.load(as: Int32.self)
        throw ReceiveError.serverError(code: Int(code))
      }

      throw ReceiveError.invalidReply
    }

    return message
  }

  private init(buffer: Data, layout: MessageLayout) {
    self.buffer = buffer
    self.layout = layout
    self.deallocated = false
  }

  deinit {
    guard !deallocated else { return }

    for (index, item) in layout.items.enumerated() {
      let offset = layout.offsetOfItem(at: index)

      withBufferData { pointer -> Void in
        switch item {
        case .port:
          let descriptor = pointer
            .advanced(by: offset)
            .assumingMemoryBound(to: mach_msg_port_descriptor_t.self)
            .pointee

          switch Int32(descriptor.disposition) {
          case MACH_MSG_TYPE_MOVE_SEND_ONCE:
            mach_port_deallocate(mach_task_self_, descriptor.name)
          default:
            break
          }

        case .outlineData:
          let ool = pointer
            .advanced(by: offset)
            .assumingMemoryBound(to: mach_msg_ool_descriptor_t.self)
            .pointee

          guard ool.deallocate != 0, var address = ool.address else { return }

          let vmAddress = withUnsafeBytes(of: &address) { $0.load(as: mach_vm_address_t.self) }

          let status = mach_vm_deallocate(mach_task_self_, vmAddress, mach_vm_size_t(ool.size))

          if status != 0 {
            print("deallocate: \(String(cString: mach_error_string(status)))")
          }
        }
      }
    }
  }

  private func withBufferData<T>(_ block: (UnsafeMutableRawPointer) throws -> T) rethrows -> T {
    return try buffer.withUnsafeMutableBytes { pointer -> T in
      try block(pointer.baseAddress!)
    }
  }

  var buffer: Data
  var deallocated: Bool
}

public enum MachSendDisposition {
  case copySend(MachSendPort)
  case makeSend(MachReceivePort)
  case makeSendOnce(MachReceivePort)
  case moveSendOnce(MachSendOncePort)

  var name: mach_port_name_t {
    switch self {
    case let .copySend(port):
      return port.name

    case let .makeSend(port), let .makeSendOnce(port):
      return port.name

    case let .moveSendOnce(port):
      return port.name
    }
  }

  var rawType: mach_msg_type_name_t {
    switch self {
    case .copySend:
      return mach_msg_type_name_t(MACH_MSG_TYPE_COPY_SEND)

    case .makeSend:
      return mach_msg_type_name_t(MACH_MSG_TYPE_MAKE_SEND)

    case .makeSendOnce:
      return mach_msg_type_name_t(MACH_MSG_TYPE_MAKE_SEND_ONCE)

    case .moveSendOnce:
      return mach_msg_type_name_t(MACH_MSG_TYPE_MOVE_SEND_ONCE)
    }
  }
}

extension Message {
  public enum Item {
    case outlineData(Data)
    case port(MachSendDisposition)

    var layoutItem: MessageLayout.Item {
      switch self {
      case .outlineData: return .outlineData
      case .port: return .port
      }
    }

    public static func codable(_ item: MachEncodable) -> Item {
      let encoder = MachEncoder()
      item.encode(with: encoder)
      return .outlineData(encoder.data)
    }
  }

  public static func send(id: mach_msg_id_t,
                          remotePort: MachSendPort,
                          localPort: MachReceivePort? = nil,
                          items: [Item] = [],
                          plainData: Data? = nil) throws {
    let local: MachSendDisposition?

    if let localPort = localPort {
      local = .makeSendOnce(localPort)
    } else {
      local = nil
    }

    try send(id: id, remote: .copySend(remotePort), local: local, items: items, plainData: plainData)
  }

  public static func send(id: mach_msg_id_t,
                          remote: MachSendDisposition,
                          local: MachSendDisposition? = nil,
                          items: [Item] = [],
                          plainData: Data? = nil) throws {
    let message = encode(id: id, remote: remote, local: local, items: items, plainData: plainData)

    try message.withBufferData { buffer in
      let headerPointer = buffer.assumingMemoryBound(to: mach_msg_header_t.self)
      let options: mach_msg_option_t = MACH_SEND_MSG

      let status = mach_msg(headerPointer, options, mach_msg_size_t(message.layout.size), 0, 0, 0, 0)

      guard status == KERN_SUCCESS else {
        throw SendError.machError(code: Int(status))
      }
    }

    message.deallocated = true
  }

  private static func encode(id: mach_msg_id_t,
                             remote: MachSendDisposition,
                             local: MachSendDisposition? = nil,
                             items: [Item] = [],
                             plainData: Data? = nil) -> Message {
    var buffer = Data()

    let layout = MessageLayout(items: items.map { $0.layoutItem }, plainDataSize: plainData?.count ?? 0)

    var header = mach_msg_header_t(
      msgh_bits: bits(remote: remote, local: local, complex: !items.isEmpty),
      msgh_size: mach_msg_size_t(layout.size),
      msgh_remote_port: remote.name,
      msgh_local_port: local?.name ?? 0,
      msgh_voucher_port: 0,
      msgh_id: id
    )

    withUnsafePointer(to: &header, { buffer.append(UnsafeBufferPointer(start: $0, count: 1)) })

    if !items.isEmpty {
      var body = mach_msg_body_t(msgh_descriptor_count: mach_msg_size_t(items.count))

      withUnsafePointer(to: &body, { buffer.append(UnsafeBufferPointer(start: $0, count: 1)) })

      for item in items {
        buffer.append(encode(item))
      }
    }

    if let plainData = plainData {
      buffer.append(plainData)
    }

    assert(buffer.count == layout.size)

    return Message(buffer: buffer, layout: layout)
  }

  private static func encode(_ item: Item) -> Data {
    switch item {
    case let .outlineData(data):
      var descriptor = allocateOutlineDataDescriptor(for: data)
      return withUnsafePointer(to: &descriptor, { Data(buffer: UnsafeBufferPointer(start: $0, count: 1)) })

    case let .port(disposition):
      var descriptor = makePortDescriptor(disposition: disposition)
      return withUnsafePointer(to: &descriptor, { Data(buffer: UnsafeBufferPointer(start: $0, count: 1)) })
    }
  }

  private static func allocateOutlineDataDescriptor(for data: Data) -> mach_msg_ool_descriptor_t {
    var vmAddress: mach_vm_address_t = 0

    mach_vm_allocate(mach_task_self_, &vmAddress, mach_vm_size_t(data.count), VM_FLAGS_ANYWHERE)

    let bufferPointer = UnsafeMutableRawPointer(OpaquePointer(bitPattern: UInt(vmAddress)))

    data.withUnsafeBytes { dataPointer -> Void in
      memcpy(bufferPointer, dataPointer.baseAddress!, data.count)
    }

    return mach_msg_ool_descriptor_t(
      address: bufferPointer,
      deallocate: 1,
      copy: mach_msg_copy_options_t(MACH_MSG_VIRTUAL_COPY),
      pad1: 0,
      type: mach_msg_descriptor_type_t(MACH_MSG_OOL_DESCRIPTOR),
      size: mach_msg_size_t(data.count)
    )
  }

  private static func makePortDescriptor(disposition: MachSendDisposition) -> mach_msg_port_descriptor_t {
    return mach_msg_port_descriptor_t(
      name: disposition.name,
      pad1: 0, pad2: 0,
      disposition: disposition.rawType,
      type: mach_msg_descriptor_type_t(MACH_MSG_PORT_DESCRIPTOR)
    )
  }

  static func bits(remote: MachSendDisposition? = nil, local: MachSendDisposition? = nil, complex: Bool = false) -> mach_msg_bits_t {
    var bits: mach_msg_bits_t = 0

    if let remote = remote {
      bits |= remote.rawType
    }

    if let local = local {
      bits |= local.rawType << 8
    }

    if complex {
      bits |= MACH_MSGH_BITS_COMPLEX
    }

    return bits
  }
}

enum MachPortRight {
  case receive
  case send

  var rawValue: mach_port_right_t {
    switch self {
    case .receive:
      return MACH_PORT_RIGHT_RECEIVE

    case .send:
      return MACH_PORT_RIGHT_SEND
    }
  }
}

public class MachReceivePort {
  public let name: mach_port_name_t

  public static func allocate() -> MachReceivePort {
    var port: mach_port_name_t = 0
    let status = mach_port_allocate(mach_task_self_, MACH_PORT_RIGHT_RECEIVE, &port)
    guard status == 0 else {
      let error = NSError(domain: NSMachErrorDomain, code: Int(status), userInfo: nil)
      fatalError(error.localizedDescription)
    }
    return MachReceivePort(name: port)
  }

  private init(name: mach_port_name_t) {
    self.name = name
  }

  deinit {
    let status = mach_port_mod_refs(mach_task_self_, name, MACH_PORT_RIGHT_RECEIVE, -1)

    if status != 0 {
      print("deallocate receive port: \(String(cString: mach_error_string(status)))")
    }
  }
}

public class MachSendPort {
  public let name: mach_port_name_t

  public static func lookup(name: String) throws -> MachSendPort {
    var port: mach_port_name_t = 0
    let status = bootstrap_look_up(bootstrap_port, name, &port)
    guard status == KERN_SUCCESS else {
      throw NSError(domain: NSMachErrorDomain, code: Int(status), userInfo: nil)
    }
    return MachSendPort(name: port)
  }

  init(_ descriptor: inout mach_msg_port_descriptor_t) throws {
    assert(descriptor.disposition == MACH_MSG_TYPE_MOVE_SEND)

    self.name = descriptor.name

    descriptor.name = 0
  }

  private init(name: mach_port_name_t) {
    self.name = name
  }

  deinit {
    let status = mach_port_mod_refs(mach_task_self_, name, MACH_PORT_RIGHT_SEND, -1)

    if status != 0 {
      print("deallocate send port: \(String(cString: mach_error_string(status)))")
    }
  }
}

public class MachSendOncePort {
  public let name: mach_port_name_t

  init(_ descriptor: inout mach_msg_port_descriptor_t) throws {
    assert(descriptor.disposition == MACH_MSG_TYPE_MOVE_SEND_ONCE)

    self.name = descriptor.name

    descriptor.name = 0
  }

  private init(name: mach_port_name_t) {
    self.name = name
  }

  deinit {
    mach_port_mod_refs(mach_task_self_, name, MACH_PORT_RIGHT_SEND_ONCE, -1)
  }

  static func construct(name: mach_port_name_t) -> MachSendOncePort {
    return .init(name: name)
  }
}

extension Message {
  public static func sendWithReply<T: MachDecodable>(remotePort: MachSendPort, messageId: mach_msg_id_t, items: [Message.Item] = [], plainData: Data? = nil) -> Promise<T> {
    return sendWithReplyRaw(
      remotePort: remotePort,
      messageId: messageId,
      items: items,
      plainData: plainData,
      replyLayout: MessageLayout(items: [.outlineData])
    ).then { message in
      let data = message.extractOutlineData(at: 0)

      return try data.withUnsafeBytes { pointer in
        let decoder = MachDecoder(buffer: pointer)
        return try T(from: decoder)
      }
    }
  }

  public static func sendWithReply(remotePort: MachSendPort, messageId: mach_msg_id_t, items: [Message.Item] = [], plainData: Data? = nil) -> Promise<Void> {
    return sendWithReplyRaw(remotePort: remotePort, messageId: messageId, items: items, plainData: plainData, replyLayout: MessageLayout()).then { _ in () }
  }

  public static func sendWithReplyRaw(remotePort: MachSendPort,
                                      messageId: mach_msg_id_t,
                                      items: [Message.Item] = [],
                                      plainData: Data? = nil,
                                      replyLayout: MessageLayout) -> Promise<Message> {
    return Promise {
      let replyPort = MachReceivePort.allocate()

      try Message.send(id: messageId, remotePort: remotePort, localPort: replyPort, items: items, plainData: plainData)

      var previous = mach_port_t(MACH_PORT_NULL)
      mach_port_request_notification(mach_task_self_, replyPort.name, MACH_NOTIFY_NO_SENDERS, 1, replyPort.name, mach_msg_type_name_t(MACH_MSG_TYPE_MAKE_SEND_ONCE), &previous)

      return try Message.receive(layout: replyLayout, id: messageId + 100, port: replyPort.name)
    }
  }
}
