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

#pragma once

#include <mach/message.hpp>
#include <mcom/file.hpp>

namespace mach {

mcom::Result<SendRight> MakeFileport(const mcom::File &file);

mcom::Result<mcom::File> OpenFileport(SendRight fileport);

template <>
struct message_codable<mcom::File> {
  mach_msg_port_descriptor_t pack(const mcom::File &file);

  mcom::File unpack(mach_msg_port_descriptor_t &descriptor);
};

}  // namespace mach
