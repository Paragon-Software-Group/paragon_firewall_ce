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
// Created by Alexey Antipov on 03.10.2019.
//

#include "extension.hpp"

#import <Foundation/Foundation.h>
#import <NetworkExtension/NetworkExtension.h>
#import <objc/runtime.h>

#include <map>
#include <memory>

#include <bsm/libbsm.h>

#import <mcom/process.hpp>

namespace {

pid_t PidFromTokenData(NSData *data) {
  return audit_token_to_pid(*static_cast<const audit_token_t *>(data.bytes));
}

static nf::AccessCheckHandler global_handler_;

template <class Completion>
nf::AccessStatus HandlePacket(const nf::Application &application, Completion &&completion) {
  if (!global_handler_) {
    return nf::AccessStatus::Allow;
  }

  return global_handler_(application, std::forward<Completion>(completion));
}

mcom::Sync<std::optional<PacketHandler>> packet_handler_;

}  // namespace

void EnableNetworkExtension() {
  @autoreleasepool {
    [NEProvider startSystemExtensionMode];
  }
}

void SetAccessCheckHandler(nf::AccessCheckHandler handler) { global_handler_ = std::move(handler); }

void SetPacketHandler(PacketHandler handler) {
  packet_handler_.Use([&](auto &handler_) { handler_ = std::move(handler); });
}

void ResetPacketHandler() {
  packet_handler_.Use([](auto &handler_) { handler_.reset(); });
}

@interface NFApplication : NSObject {
 @public
  std::optional<nf::Application> application_;
}
@end

@implementation NFApplication
- (instancetype)initWithFlow:(NEFilterFlow *)flow {
  if (flow == nil) {
    return nil;
  }

  auto token_data = flow.sourceAppAuditToken;
  if (!token_data) {
    return nil;
  }

  mcom::Result<mcom::FilePath> path = mcom::ProcessPath(PidFromTokenData(token_data));
  if (!path) {
    return nil;
  }

  self = [super init];

  application_.emplace(path->String());

  return self;
}
@end

@interface NEFilterFlow (AppInfoCache)
@property(nonatomic, readonly) std::optional<nf::Application> nfApplication;
@end

@implementation NEFilterFlow (AppInfoCache)
static int kAppAssociation = 0;

- (std::optional<nf::Application>)nfApplication {
  id object = objc_getAssociatedObject(self, &kAppAssociation);

  if (object == nil) {
    auto application = [[NFApplication alloc] initWithFlow:self];
    if (application == nil) {
      objc_setAssociatedObject(self, &kAppAssociation, [NSNull null], OBJC_ASSOCIATION_ASSIGN);
      return std::nullopt;
    }
    objc_setAssociatedObject(self, &kAppAssociation, application,
                             OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    return application->application_;
  }

  if (object == [NSNull null]) {
    return std::nullopt;
  } else {
    return static_cast<NFApplication *>(object)->application_;
  }
}
@end

@interface FilterDataProvider : NEFilterDataProvider

@end

@implementation FilterDataProvider
- (void)startFilterWithCompletionHandler:(void (^)(NSError *_Nullable))completionHandler {
  auto hosts = [[NSMutableArray alloc] initWithArray:@[
    @"apps.mzstatic.com", @"bag.itunes.apple.com", @"buy.itunes.apple.com",
    @"init.itunes.apple.com", @"osxapps.itunes.apple.com", @"pd.itunes.app",
    @"play.itunes.apple.com", @"se-edge.itunes.apple.com", @"xp.apple.com"
  ]];

  for (unsigned i = 0; i < 100; ++i) {
    [hosts addObject:[NSString stringWithFormat:@"p%u-buy.itunes.apple.com", i]];
  }

  auto rules = [NSMutableArray new];

  for (NSString *host in hosts) {
    auto endpoint = [NWHostEndpoint endpointWithHostname:host port:@"0"];
    auto network_rule = [[NENetworkRule alloc] initWithDestinationHost:endpoint
                                                              protocol:NENetworkRuleProtocolAny];
    auto filter_rule = [[NEFilterRule alloc] initWithNetworkRule:network_rule
                                                          action:NEFilterActionAllow];
    [rules addObject:filter_rule];
  }

  auto settings = [[NEFilterSettings alloc] initWithRules:rules
                                            defaultAction:NEFilterActionFilterData];

  [self applySettings:settings
      completionHandler:^(NSError *_Nullable error) {
        completionHandler(error);
      }];
}

- (void)stopFilterWithReason:(NEProviderStopReason)reason
           completionHandler:(void (^)())completionHandler {
  completionHandler();
}

- (NEFilterNewFlowVerdict *)handleNewFlow:(NEFilterFlow *)flow {
  const auto status = [self handleFlow:flow];
  if (!status) {
    return [NEFilterNewFlowVerdict allowVerdict];
  }

  switch (*status) {
    case nf::AccessStatus::Allow:
      return packet_handler_.Use([](auto handler) {
        if (handler) {
          return
              [NEFilterNewFlowVerdict filterDataVerdictWithFilterInbound:YES
                                                        peekInboundBytes:handler->max_flow_bytes
                                                          filterOutbound:YES
                                                       peekOutboundBytes:handler->max_flow_bytes];
        } else {
          return [NEFilterNewFlowVerdict allowVerdict];
        }
      });
    case nf::AccessStatus::Deny:
      return [NEFilterNewFlowVerdict dropVerdict];
    case nf::AccessStatus::Wait:
      return [NEFilterNewFlowVerdict pauseVerdict];
  }
}

- (NEFilterDataVerdict *)handleInboundDataFromFlow:(NEFilterFlow *)flow
                              readBytesStartOffset:(NSUInteger)offset
                                         readBytes:(NSData *)readBytes {
  auto application = [self applicationForFlow:flow];
  if (!application) {
    return [NEFilterDataVerdict allowVerdict];
  }
  const auto status = [self checkAccessForFlow:flow application:*application];

  switch (status) {
    case nf::AccessStatus::Allow:
      return packet_handler_.Use([&](auto handler) {
        if (handler) {
          handler->handler({static_cast<uint32_t>(readBytes.length),
                            nf::Packet::Direction::Incoming, *application});
          return [NEFilterDataVerdict dataVerdictWithPassBytes:readBytes.length
                                                     peekBytes:handler->max_flow_bytes];
        } else {
          return [NEFilterDataVerdict allowVerdict];
        }
      });
    case nf::AccessStatus::Deny:
      return [NEFilterDataVerdict dropVerdict];
    case nf::AccessStatus::Wait:
      return [NEFilterDataVerdict pauseVerdict];
  }
}

- (NEFilterDataVerdict *)handleInboundDataCompleteForFlow:(NEFilterFlow *)flow {
  const auto status = [self handleFlow:flow];
  if (!status) {
    return [NEFilterDataVerdict allowVerdict];
  }

  switch (*status) {
    case nf::AccessStatus::Allow:
      return [NEFilterDataVerdict allowVerdict];
    case nf::AccessStatus::Deny:
      return [NEFilterDataVerdict dropVerdict];
    case nf::AccessStatus::Wait:
      return [NEFilterDataVerdict pauseVerdict];
  }
}

- (NEFilterDataVerdict *)handleOutboundDataFromFlow:(NEFilterFlow *)flow
                               readBytesStartOffset:(NSUInteger)offset
                                          readBytes:(NSData *)readBytes {
  const auto application = [self applicationForFlow:flow];
  if (!application) {
    return [NEFilterDataVerdict allowVerdict];
  }
  const auto status = [self checkAccessForFlow:flow application:*application];

  switch (status) {
    case nf::AccessStatus::Allow:
      return packet_handler_.Use([&](auto handler) {
        if (handler) {
          handler->handler({static_cast<uint32_t>(readBytes.length),
                            nf::Packet::Direction::Outgoing, *application});
          return [NEFilterDataVerdict dataVerdictWithPassBytes:readBytes.length
                                                     peekBytes:handler->max_flow_bytes];
        } else {
          return [NEFilterDataVerdict allowVerdict];
        }
      });
    case nf::AccessStatus::Deny:
      return [NEFilterDataVerdict dropVerdict];
    case nf::AccessStatus::Wait:
      return [NEFilterDataVerdict pauseVerdict];
  }
}

- (NEFilterDataVerdict *)handleOutboundDataCompleteForFlow:(NEFilterFlow *)flow {
  const auto status = [self handleFlow:flow];
  if (!status) {
    return [NEFilterDataVerdict allowVerdict];
  }

  switch (*status) {
    case nf::AccessStatus::Allow:
      return [NEFilterDataVerdict allowVerdict];
    case nf::AccessStatus::Deny:
      return [NEFilterDataVerdict dropVerdict];
    case nf::AccessStatus::Wait:
      return [NEFilterDataVerdict pauseVerdict];
  }
}

- (std::optional<nf::Application>)applicationForFlow:(NEFilterFlow *)flow {
  if (!flow) {
    return std::nullopt;
  }

  static nf::Application webkit_service{
      "/System/Library/Frameworks/WebKit.framework/Versions/A/XPCServices/"
      "com.apple.WebKit.Networking.xpc/Contents/MacOS/com.apple.WebKit.Networking"};
  static nf::Application safari_app{"/Applications/Safari.app/Contents/MacOS/Safari"};

  std::optional<nf::Application> application = flow.nfApplication;

  if (application == webkit_service) {
    return safari_app;
  }

  return application;
}

- (std::optional<nf::AccessStatus>)handleFlow:(NEFilterFlow *)flow {
  const auto application = [self applicationForFlow:flow];
  if (!application) {
    return std::nullopt;
  }

  return [self checkAccessForFlow:flow application:*application];
}

- (nf::AccessStatus)checkAccessForFlow:(NEFilterFlow *)flow
                           application:(const nf::Application &)application {
  return HandlePacket(application, [self, flow](auto access_status) {
    auto verdict = (access_status == nf::AccessStatus::Allow)
                       ? [NEFilterNewFlowVerdict allowVerdict]
                       : [NEFilterNewFlowVerdict dropVerdict];
    [self resumeFlow:flow withVerdict:verdict];
  });
}
@end
