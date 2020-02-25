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
// Created by Alexey Antipov on 2019-12-16.
//

import Foundation

public protocol PropertyListType {}

extension Bool: PropertyListType {}
extension UInt32: PropertyListType {}
extension Int: PropertyListType {}

@propertyWrapper
public struct Defaults<T> {
  public typealias Encoder = (T) -> Any
  public typealias Decoder = (Any) -> T?

  public let key: String

  public init(_ key: String, defaults: UserDefaults = .standard, encode: @escaping Encoder, decode: @escaping Decoder) {
    self.key = key
    self.defaults = defaults
    self.encoder = encode
    self.decoder = decode
  }

  public var wrappedValue: T? {
    get {
      defaults.object(forKey: key).flatMap(decoder)
    }
    set {
      defaults.set(newValue.map(encoder), forKey: key)
    }
  }

  private let defaults: UserDefaults
  private let encoder: Encoder
  private let decoder: Decoder
}

public extension Defaults where T: PropertyListType {
  init(_ key: String, defaults: UserDefaults = .standard) {
    self.init(key, defaults: defaults, encode: { $0 }, decode: { $0 as? T })
  }
}

public extension Defaults where T: RawRepresentable, T.RawValue: PropertyListType {
  init(rawValue key: String, defaults: UserDefaults = .standard) {
    self.init(key, defaults: defaults, encode: { $0.rawValue }, decode: { ($0 as? T.RawValue).flatMap { T(rawValue: $0) } })
  }
}

public extension Defaults where T: Codable {
  init(json key: String, defaults: UserDefaults = .standard) {
    self.init(key, defaults: defaults, encode: { value in
      (try? JSONEncoder().encode(value)) as Any
    }, decode: { value in
      (value as? Data).flatMap { try? JSONDecoder().decode(T.self, from: $0) }
    })
  }
}
