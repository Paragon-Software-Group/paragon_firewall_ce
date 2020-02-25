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
// Created by Alexey Antipov on 01.07.2019.
//

import Combine
import Foundation
import SystemExtensions
import os.log

struct SystemExtensionInfo {
    let identifier: String
    let bundle: Bundle
    let machServiceName: String
}

typealias SystemExtensionRequestResult = Result<OSSystemExtensionRequest.Result, Error>
typealias SystemExtensionRequestCompletion = (SystemExtensionRequestResult) -> Void
typealias SystemExtensionRequestApproval = (Bool) -> Void

private class SystemExtensionRequestDelegate: NSObject, OSSystemExtensionRequestDelegate {
    typealias Completion = SystemExtensionRequestCompletion
    typealias Approval = SystemExtensionRequestApproval
    
    let completion: Completion
    let approval: Approval?
    var selfAnchor: AnyObject?
    
    init(needApproval callback: Approval?, completion: @escaping Completion) {
        self.completion = completion
        self.approval = callback
        super.init()
        self.selfAnchor = self
    }
    
    func completed(_ result: SystemExtensionRequestResult) {
        completion(result)
        approval?(false)
        selfAnchor = nil
    }
    
    func request(_ request: OSSystemExtensionRequest, actionForReplacingExtension existing: OSSystemExtensionProperties, withExtension ext: OSSystemExtensionProperties) -> OSSystemExtensionRequest.ReplacementAction {
        return .replace
    }
    
    func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
        approval?(true)
    }
    
    func request(_ request: OSSystemExtensionRequest, didFinishWithResult result: OSSystemExtensionRequest.Result) {
        completed(.success(result))
    }
    
    func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
        completed(.failure(error))
    }
}

enum SystemExtensionError: Error {
    case rebootRequired
    case unknownResult(OSSystemExtensionRequest.Result)
}

extension OSSystemExtensionManager {
    func activateExtensionWithIdentifier(_ identifier: String, approval callback: SystemExtensionRequestApproval? = nil) -> Future<Void, Error> {
        return processRequest(.activationRequest(forExtensionWithIdentifier: identifier, queue: .global()), approval: callback)
    }
    
    func deactivateExtensionWithIdentifier(_ identifier: String) -> AnyPublisher<Void, Error> {
      processRequest(.deactivationRequest(forExtensionWithIdentifier: identifier, queue: .global()), approval: nil)
            .catch { error -> Result<Void, Error>.Publisher in
                if let sysExtError = error as? OSSystemExtensionError,
                    sysExtError.code == .extensionNotFound {
                    return Result.success(Void()).publisher
                }
                return Result.failure(error).publisher
            }
            .eraseToAnyPublisher()
    }
    
    private func processRequest(_ request: OSSystemExtensionRequest, approval callback: SystemExtensionRequestApproval?) -> Future<Void, Error> {
        os_log("submit system extension request for %@", request.identifier)
        
        return Future { promise in
            let delegate = SystemExtensionRequestDelegate(needApproval: callback) { result in
                os_log("did finish system extension request with result: %{public}@",
                       String(describing: result))
                switch result {
                case .success(let result):
                    switch result {
                    case .completed:
                        promise(.success(()))
                    case .willCompleteAfterReboot:
                        promise(.failure(SystemExtensionError.rebootRequired))
                    @unknown default:
                        promise(.failure(SystemExtensionError.unknownResult(result)))
                    }
                case .failure(let error):
                    promise(.failure(error))
                }
            }
            request.delegate = delegate
            self.submitRequest(request)
        }
    }
}

extension Bundle {
    var systemExtensions: [SystemExtensionInfo] {
        let extensionsURL = Bundle.main.bundleURL
            .appendingPathComponent("Contents/Library/SystemExtensions")
        
        guard let extensionsURLs = try? FileManager.default.contentsOfDirectory(
            at: extensionsURL,
            includingPropertiesForKeys: nil,
            options: .skipsHiddenFiles) else { return [] }
        
        return extensionsURLs.compactMap { url in
            guard url.pathExtension == "systemextension",
                let bundle = Bundle(url: url),
                let identifier = bundle.bundleIdentifier,
                let networkExtensionInfo = bundle.object(forInfoDictionaryKey: "NetworkExtension") as? [String: Any],
                let machServiceName = networkExtensionInfo["NEMachServiceName"] as? String
                else { return nil }
            
            return SystemExtensionInfo(
                identifier: identifier,
                bundle: bundle,
                machServiceName: machServiceName)
        }
    }
}
