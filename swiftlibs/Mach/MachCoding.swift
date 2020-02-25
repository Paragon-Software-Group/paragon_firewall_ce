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

public protocol MachEncodable {
  func encode(with encoder: MachEncoder)
}

public protocol MachDecodable {
  init(from decoder: MachDecoder) throws
}

public protocol MachCodable: MachEncodable & MachDecodable {}

public class MachEncoder {
  public private(set) var data = Data()

  public init() {}

  public func encodeInt32(_ value: Int32) {
    encodeTrivial(value)
  }

  public func encodeString(_ string: String) {
    string.withCString { cString in
      let size = strlen(cString)
      encodeInt32(Int32(size))

      let u8Pointer = UnsafePointer<UInt8>(OpaquePointer(cString))
      data.append(u8Pointer, count: size)

      alignEnd()
    }
  }

  public func encodeTrivial<T>(_ value: T) {
    var value = value

    withUnsafePointer(to: &value) { (pointer: UnsafePointer<T>) in
      data.append(UnsafeBufferPointer<T>(start: pointer, count: 1))
    }

    alignEnd()
  }

  public func encodeOptional<T>(_ value: T?) where T: MachEncodable {
    encodeInt32(value != nil ? 1 : 0)
    value.map { $0.encode(with: self) }
  }

  func alignEnd() {
    let alignment = (4 - (data.count % 4)) % 4

    guard alignment != 0 else { return }

    data.append(contentsOf: [UInt8](repeating: 0, count: alignment))
  }
}

public class MachDecoder {
  public init(buffer: UnsafeRawBufferPointer) {
    address = buffer.baseAddress!
    size = buffer.count
  }

  public func decodeInt32() throws -> Int32 {
    return try decodeBasic()
  }

  public func decodeInt64() throws -> Int64 {
    return try decodeBasic()
  }

  public func decodeDouble() throws -> Double {
    return try decodeBasic()
  }

  public func decodeString() throws -> String {
    let size = try Int(decodeInt32())
    let data = readBytes(size: size)
    let bytes = UnsafePointer<UInt8>(OpaquePointer(data))
    let buffer = UnsafeBufferPointer<UInt8>(start: bytes, count: size)
    return String(bytes: buffer, encoding: .utf8)!
  }

  public func decodeBasic<T>() throws -> T {
    return readBytes(size: MemoryLayout<T>.size).assumingMemoryBound(to: T.self).pointee
  }

  public func decodeOptional<T: MachDecodable>() throws -> T? {
    let hasValue = (try decodeInt32()) != 0

    if hasValue {
      return try T(from: self)
    } else {
      return nil
    }
  }

  public func readBytes(size readSize: Int) -> UnsafeRawPointer {
    guard size >= readSize else { fatalError() }

    let base = address
    let alignedSize = (readSize + 3) & ~3
    address = address.advanced(by: alignedSize)
    return base
  }

  private var address: UnsafeRawPointer
  private var size: Int
}

extension String: MachEncodable {
  public func encode(with encoder: MachEncoder) {
    encoder.encodeString(self)
  }
}

extension Array: MachEncodable where Element: MachEncodable {
  public func encode(with encoder: MachEncoder) {
    encoder.encodeInt32(Int32(count))

    for item in self {
      item.encode(with: encoder)
    }
  }
}

extension Array: MachDecodable where Element: MachDecodable {
  public init(from decoder: MachDecoder) throws {
    let count = try Int(decoder.decodeInt32())

    self = try (0 ..< count).map({ _ in try Element(from: decoder) })
  }
}

extension Data {
  public static func withUnsafeBytes<T>(of value: T) -> Data {
    return withUnsafePointer(to: value) { Data(buffer: UnsafeBufferPointer(start: $0, count: 1)) }
  }
}
