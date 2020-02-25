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
// Created by Alexey Antipov on 09.09.2019.
//

import Foundation
import Combine
import NetworkExtension
import SystemExtensions
import Mach
import NetworkFilter
import nf

enum ActivationError: Error {
  case invalidExtensionVersion(Data, bundled: Data)
  case checkVersionTimeout
}

func checkOSStatus(_ status: OSStatus) throws {
  if status == 0 { return }
  
  throw NSError(domain: NSOSStatusErrorDomain, code: Int(status), userInfo: nil)
}

func getCDHash(_ bundle: Bundle) throws -> Data {
  var staticCodeRef: SecStaticCode?
  try checkOSStatus(SecStaticCodeCreateWithPath(bundle.executableURL! as CFURL, [], &staticCodeRef))
  let staticCode = staticCodeRef!
  
  var informationRef: CFDictionary?
  try checkOSStatus(SecCodeCopySigningInformation(staticCode, [], &informationRef))
  let information = (informationRef!) as! [AnyHashable: Any]
  
  return information[kSecCodeInfoUnique] as! Data
}

func getCDHash(_ processIdentifier: pid_t) throws -> Data {
  let guestAttributes: [CFString: Any] = [kSecGuestAttributePid: processIdentifier]
  
  var guestRef: SecCode?
  try checkOSStatus(SecCodeCopyGuestWithAttributes(nil, guestAttributes as CFDictionary, [], &guestRef))
  let guest = guestRef!
  
  var staticCodeRef: SecStaticCode?
  try checkOSStatus(SecCodeCopyStaticCode(guest, [], &staticCodeRef))
  let staticCode = staticCodeRef!
  
  var informationRef: CFDictionary?
  try checkOSStatus(SecCodeCopySigningInformation(staticCode, [], &informationRef))
  let information = (informationRef!) as! [AnyHashable: Any]
  
  return information[kSecCodeInfoUnique] as! Data
}

func checkServiceVersion(extensionInfo: SystemExtensionInfo) -> Future<Void, Error> {
  .init { promise in
    do {
      let port = try MachSendPort.lookup(name: extensionInfo.machServiceName)
      
      Message.sendWithReplyRaw(
        remotePort: port,
        messageId: 250,
        items: [],
        plainData: nil,
        replyLayout: MessageLayout()
      ).then { message in
        let bundledVersion = try getCDHash(extensionInfo.bundle)
        
        let processIdentifier = pid_t(message.auditToken!.val.5)
        let extensionVersion = try getCDHash(processIdentifier)
        
        guard bundledVersion == extensionVersion else {
          throw ActivationError.invalidExtensionVersion(extensionVersion, bundled: bundledVersion)
        }
        
        promise(.success(()))
      }.catch { error in
        promise(.failure(error))
      }
    } catch {
      promise(.failure(error))
    }
    
//    let connection = NSXPCConnection(machServiceName: extensionInfo.machServiceName)
//    connection.remoteObjectInterface = NSXPCInterface(with: CtlTransferProto.self)
//    connection.resume()
//
//    let service = connection.remoteObjectProxyWithErrorHandler { error in
//      promise(.failure(error))
//    } as! CtlTransferProto
//
//    service.connect {
//      _ = service
//
//      do {
//        let bundledVersion = try getCDHash(extensionInfo.bundle)
//        let extensionVersion = try getCDHash(connection.processIdentifier)
//
//        guard bundledVersion == extensionVersion else {
//          throw ActivationError.invalidExtensionVersion(extensionVersion, bundled: bundledVersion)
//        }
//
//        promise(.success(()))
//      } catch {
//        promise(.failure(error))
//      }
//    }
//
//    DispatchQueue.global().asyncAfter(deadline: .now() + .seconds(20)) {
//      promise(.failure(ActivationError.checkVersionTimeout))
//    }
  }
}

private func saveNetworkExtensionConfig(extensionIdentifier: String, enabled: Bool) -> Future<Void, Error> {
  .init { promise in
    let manager = NEFilterManager.shared()
    manager.loadFromPreferences { loadError in
      if let error = loadError {
        promise(.failure(error))
        return
      }

      // don't try to disable disabled service
      if !enabled && !manager.isEnabled {
        promise(.success(()))
        return
      }
      
      let config = NEFilterProviderConfiguration()
      config.filterPackets = false
      config.filterSockets = true
      config.filterDataProviderBundleIdentifier = extensionIdentifier
      
      manager.providerConfiguration = config
      manager.isEnabled = enabled
      manager.grade = .firewall
      manager.localizedDescription = "Paragon Firewall"
      
      manager.saveToPreferences { saveError in
        if let error = saveError {
          promise(.failure(error))
        } else {
          promise(.success(()))
        }
      }
    }
  }
}

func enableNetworkExtension(extensionIdentifier: String) -> Future<Void, Error> {
  saveNetworkExtensionConfig(extensionIdentifier: extensionIdentifier, enabled: true)
}

func disableNetworkExtension(extensionIdentifier: String) -> Future<Void, Error> {
  saveNetworkExtensionConfig(extensionIdentifier: extensionIdentifier, enabled: false)
}

extension Publisher {
  func handleSubscription(_ handler: @escaping (String) -> Void, _ string: String) -> Publishers.HandleEvents<Self> {
    handleEvents(receiveSubscription: { _ in
      handler(string)
    }, receiveCompletion: { completion in
      if case let .failure(error) = completion {
        handler("error (\(string)): \(error)")
      }
    })
  }
}

extension Publisher where Failure : Error {
  func ignoreNonFatalError(value: Output, _ isFatal: @escaping (Failure) -> Bool) -> Publishers.Catch<Self, Result<Output, Failure>.Publisher> {
    self.catch { error in
      return isFatal(error) ? .init(error) : .init(value)
    }
  }
}

extension Publisher where Output == Void, Failure : Error {
  func ignoreNonFatalError(_ isFatal: @escaping (Failure) -> Bool) -> Publishers.Catch<Self, Result<Void, Failure>.Publisher> {
    ignoreNonFatalError(value: (), isFatal)
  }
}

extension Publisher where Failure : Error {
  func handleNonFatalError<P>(_ isFatal: @escaping (Failure) -> Bool, _ handler: @escaping () -> P) -> Publishers.TryCatch<Self, P> where P : Publisher, P.Output == Output {
    tryCatch { error -> P in
      guard !isFatal(error) else { throw error }
      return handler()
    }
  }
}

func activate(extensionInfo: SystemExtensionInfo, mode: FilterResult, rules: [Rule], logger: @escaping (String) -> Void, approval: @escaping SystemExtensionRequestApproval) -> AnyPublisher<NetworkFilterManager, Error> {
  func checkVersion() -> AnyPublisher<Void, Error> {
    checkServiceVersion(extensionInfo: extensionInfo)
      .handleEvents(receiveSubscription: { _ in
        logger("check service version")
      }, receiveCompletion: { completion in
        if case let .failure(error) = completion {
          logger("version check error: \(error.localizedDescription)")
        }
      })
      .eraseToAnyPublisher()
  }
  
  func deactivateExtension() -> AnyPublisher<Void, Error> {
    OSSystemExtensionManager.shared.deactivateExtensionWithIdentifier(extensionInfo.identifier)
      .handleEvents(receiveSubscription: { _ in
        logger("deactivate system extension")
      }, receiveCompletion: { completion in
        if case let .failure(error) = completion {
          logger("system extension deactivation error: \(error.localizedDescription)")
        }
      })
      .eraseToAnyPublisher()
  }
  
  func activateExtension() -> AnyPublisher<Void, Error> {
    OSSystemExtensionManager.shared.activateExtensionWithIdentifier(extensionInfo.identifier, approval: approval)
      .handleEvents(receiveSubscription: { _ in
        logger("activate system extension")
      }, receiveCompletion: { completion in
        if case let .failure(error) = completion {
          logger("system extension activation error: \(error.localizedDescription)")
        }
      })
      .eraseToAnyPublisher()
  }
  
  func after<P : Publisher>(_ period: DispatchQueue.SchedulerTimeType.Stride, run publisher: @escaping (() -> P)) -> AnyPublisher<P.Output, P.Failure> where P.Output == Void {
    Just<P.Output>(())
      .delay(for: period, scheduler: DispatchQueue.global())
      .setFailureType(to: P.Failure.self)
      .flatMap(publisher)
      .eraseToAnyPublisher()
  }
  
  func isPermissionDenied(_ error: Error) -> Bool {
    if (error as? OSSystemExtensionError)?.code == .authorizationRequired {
      return true
    }
    
    let nsError = error as NSError
    if nsError.domain == NEFilterErrorDomain, nsError.code == NEFilterManagerError.configurationPermissionDenied.rawValue {
      return true
    }
    
    return false
  }
  
  let activateAndCheckVersion = deferred(activateExtension)
    .flatMap(checkVersion)
    .handleNonFatalError(isPermissionDenied, {
      after(.seconds(2), run: { deferred(activateExtension).flatMap(checkVersion) })
    })
  
  let reloadAndCheckVersion = deferred(deactivateExtension)
    .flatMap(activateExtension)
    .flatMap(checkVersion)
  
  func enableNetworkExtensionAndLog() -> AnyPublisher<Void, Error> {
    Deferred {
      enableNetworkExtension(extensionIdentifier: extensionInfo.identifier)
        .handleSubscription(logger, "enable network extension")
    }.eraseToAnyPublisher()
  }
  
  return enableNetworkExtensionAndLog()
    .ignoreNonFatalError(isPermissionDenied)
    .flatMap(checkVersion)
    .handleNonFatalError(isPermissionDenied, {
      activateAndCheckVersion
        .handleNonFatalError(isPermissionDenied, { reloadAndCheckVersion })
        .flatMap(enableNetworkExtensionAndLog)
    })
    .tryMap { try ParagonNetworkFilterManager(mode: mode, rules: rules, serviceName: extensionInfo.machServiceName) }
    .eraseToAnyPublisher()
}

func deferred<P>(_ createPublisher: @escaping () -> P) -> Deferred<P> {
  return Deferred(createPublisher: createPublisher)
}
