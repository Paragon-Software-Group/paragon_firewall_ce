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
// Created by Alexey Antipov on 08/08/2018.
//

public enum Result<T> {
  case success(T)
  case failure(Error)

  public func get() throws -> T {
    switch self {
    case let .success(x):
      return x
    case let .failure(e):
      throw e
    }
  }

  public static func of(_ block: () throws -> T) -> Result<T> {
    do {
      return .success(try block())
    } catch {
      return .failure(error)
    }
  }
}

extension Result: CustomStringConvertible {
  public var description: String {
    switch self {
    case let .success(value):
      return String(describing: value)
    case let .failure(error):
      return "(error: \(error))"
    }
  }
}
