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
// Created by Alexey Antipov on 16/01/2019.
//

import Foundation

public class Promise<T> {
  public init(_ result: Result<T>? = nil) {
    self.state = Monitor(State(result: result, handlers: []))
  }

  public init(queue: DispatchQueue = .global(), _ block: @escaping () throws -> T) {
    self.state = Monitor(State(result: nil, handlers: []))

    queue.async {
      self.fulfill(Result.of(block))
    }
  }

  @discardableResult
  public func handle<R>(on queue: DispatchQueue = .global(), _ block: @escaping (Result<T>) throws -> R) -> Promise<R> {
    return state.use { state -> Promise<R> in
      if let result = state.result {
        return Promise<R>(queue: queue) {
          try block(result)
        }
      }

      let promise = Promise<R>()
      state.handlers.append { result in
        queue.async {
          promise.fulfill(Result.of({ try block(result) }))
        }
      }
      return promise
    }
  }

  @discardableResult
  public func then<R>(on queue: DispatchQueue = .global(), do block: @escaping (T) throws -> R) -> Promise<R> {
    return state.use { state -> Promise<R> in
      if let result = state.result {
        return Promise<R>(queue: queue) {
          switch result {
          case let .success(result):
            return try block(result)
          case let .failure(error):
            throw error
          }
        }
      }

      let promise = Promise<R>()
      state.handlers.append { result in
        switch result {
        case let .success(result):
          queue.async {
            promise.fulfill(Result.of({ try block(result) }))
          }
        case let .failure(error):
          promise.fulfill(.failure(error))
        }
      }
      return promise
    }
  }

  public func then<R>(_ block: @escaping (T) -> Promise<R>) -> Promise<R> {
    return state.use { state in
      if let result = state.result {
        switch result {
        case let .success(result):
          return block(result)
        case let .failure(error):
          let promise = Promise<R>()
          promise.fulfill(.failure(error))
          return promise
        }
      }

      let promise = Promise<R>()
      state.handlers.append { result in
        switch result {
        case let .success(result):
          _ = block(result).handle { result in
            promise.fulfill(result)
          }
        case let .failure(error):
          promise.fulfill(.failure(error))
        }
      }
      return promise
    }
  }

  @discardableResult
  public func wait() -> Result<T> {
    switch tryWait() {
    case let .result(result):
      return result
    case let .semaphore(sema):
      sema.wait()
      return state.use { $0.result! }
    }
  }

  public func `catch`(on queue: DispatchQueue = .global(), _ block: @escaping (Error) -> Void) {
    let handler: (Result<T>) -> Void = { result in
      if case let .failure(error) = result {
        queue.async {
          block(error)
        }
      }
    }

    state.use { state in
      if let result = state.result {
        handler(result)
      } else {
        state.handlers.append(handler)
      }
    }
  }

  public func fulfill(_ result: Result<T>) {
    state.use { state in
      state.result = result
      for handler in state.handlers {
        handler(result)
      }
      state.handlers = []
    }
  }

  public func setValue(_ value: T) {
    fulfill(.success(value))
  }

  private typealias Handler = (Result<T>) -> Void

  private struct State {
    var result: Result<T>?
    var handlers: [Handler]
  }

  private enum WaitResult {
    case result(Result<T>)
    case semaphore(DispatchSemaphore)
  }

  private func tryWait() -> WaitResult {
    return state.use { state in
      if let result = state.result {
        return .result(result)
      }

      let sema = DispatchSemaphore(value: 0)
      state.handlers.append { _ in sema.signal() }
      return .semaphore(sema)
    }
  }

  private let state: Monitor<State>
}
