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

import Foundation

public protocol MessageHandlerProtocol {
  var maxMessageSize: Int { get }

  func handleMessage(_ message: Message) -> Bool
}

public class MessageHandler: MessageHandlerProtocol {
  public typealias Handler = (Message) -> Bool

  public let maxMessageSize: Int

  public init(maxMessageSize: Int, messageId: mach_msg_id_t? = nil, handler: @escaping Handler) {
    self.maxMessageSize = maxMessageSize
    self.handler = handler
    self.layout = nil
    self.messageId = messageId
  }

  public init(layout: MessageLayout, messageId: mach_msg_id_t? = nil, handler: @escaping Handler) {
    self.maxMessageSize = layout.size
    self.layout = layout
    self.messageId = messageId
    self.handler = handler
  }

  public func handleMessage(_ message: Message) -> Bool {
    guard messageId == nil || messageId == message.header.msgh_id else { return false }

    guard layout == nil || layout == message.layout else { return false }

    return handler(message)
  }

  public static func plainDataHandler<T>(messageId: mach_msg_id_t? = nil, handler: @escaping (T) -> Void) -> MessageHandler {
    let layout = MessageLayout(plainDataSize: MemoryLayout<T>.size)

    return MessageHandler(layout: layout, messageId: messageId) { message in
      DispatchQueue.global().async {
        let value = message.plainData.load(as: T.self)

        handler(value)
      }

      return true
    }
  }

  public static func codableHandler<T: MachDecodable>(messageId: mach_msg_id_t? = nil, handler: @escaping (T) -> Void) -> MessageHandler {
    let layout = MessageLayout(items: [.outlineData])

    return MessageHandler(layout: layout, messageId: messageId) { message in
      DispatchQueue.global().async {
        let data = message.extractOutlineData(at: 0)

        data.withUnsafeBytes { pointer -> Void in
          let decoder = MachDecoder(buffer: pointer)

          if let value = try? T(from: decoder) {
            handler(value)
          }
        }
      }

      return true
    }
  }

  public static func sendOncePortHandler(messageId: mach_msg_id_t? = nil, handler: @escaping (MachSendOncePort) -> Void) -> MessageHandler {
    let layout = MessageLayout(items: [.port])

    return MessageHandler(layout: layout, messageId: messageId) { message in
      guard let port = try? message.extractSendOnceRight(at: 0) else { return false }

      DispatchQueue.global().async {
        handler(port)
      }

      return true
    }
  }

  private let handler: Handler
  private let layout: MessageLayout?
  private let messageId: mach_msg_id_t?
}

public class MachServer {
  public let port: MachReceivePort

  public init(port: MachReceivePort = MachReceivePort.allocate()) {
    self.port = port
  }

  public func start(queue: DispatchQueue? = nil) {
    guard self.source == nil else { return }

    let source = DispatchSource.makeMachReceiveSource(port: port.name, queue: queue)

    source.setEventHandler {
      self.handleMessage()
    }

    source.resume()

    self.source = source
  }

  public func stop() {
    source?.cancel()
    source = nil
  }

  public func addHandler(_ handler: MessageHandlerProtocol) {
    handlers_.append(handler)
  }

  private func handleMessage() {
    guard let maxMessageSize = handlers_.map({ $0.maxMessageSize }).max() else { return }

    do {
      let message = try Message.receive(maxSize: maxMessageSize, port: port.name)

      if nil == handlers_.first(where: { $0.handleMessage(message) }) {
        print("no handle registered to handle message with id of \(message.header.msgh_id)")
      }
    } catch {
      print("failed to receive incoming mach message: \(error.localizedDescription)")
      return
    }
  }

  private var handlers_: [MessageHandlerProtocol] = []
  private var source: DispatchSourceMachReceive?
}

public extension MachServer {
  func addCodableHandler<T: MachDecodable>(messageId: mach_msg_id_t, handler: @escaping (T) -> Void) {
    addHandler(MessageHandler.codableHandler(messageId: messageId, handler: handler))
  }

  func addCodableHandlerR<T: MachDecodable>(messageId: mach_msg_id_t, handler: @escaping (_ value: T, _ replyPort: MachSendOncePort) -> Void) {
    let layout = MessageLayout(items: [.outlineData])

    let messageHandler = MessageHandler(layout: layout, messageId: messageId) { message -> Bool in
      guard let replyPort = message.extractReplySendOnceRight() else { return false }

      let data = message.extractOutlineData(at: 0)

      guard let object = data.withUnsafeBytes({ pointer in
        try? T(from: MachDecoder(buffer: pointer))
      }) else { return false }

      handler(object, replyPort)

      return true
    }

    addHandler(messageHandler)
  }

  func addCodableHandler<T: MachDecodable>(messageId: mach_msg_id_t, handler: @escaping (T, _ promise: @escaping () -> Void) -> Void) {
    addCodableHandlerR(messageId: messageId) { value, replyPort in
      handler(value) {
        try? Message.send(id: messageId + 100, remote: .moveSendOnce(replyPort))
      }
    }
  }

  func addSendOncePortHandler(messageId: mach_msg_id_t, handler: @escaping (MachSendOncePort) -> Void) {
    addHandler(MessageHandler.sendOncePortHandler(messageId: messageId, handler: handler))
  }
}
