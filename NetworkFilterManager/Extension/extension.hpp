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

#pragma once

#include <nf/nf.hpp>

struct PacketHandler {
  uint32_t max_flow_bytes;
  std::function<void(const nf::Packet &packet)> handler;
};

void EnableNetworkExtension(void);

void SetAccessCheckHandler(nf::AccessCheckHandler packet_handler);

void SetPacketHandler(PacketHandler handler);

void ResetPacketHandler(void);
