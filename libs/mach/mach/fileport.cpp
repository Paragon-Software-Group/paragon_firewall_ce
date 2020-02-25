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

#include <mach/fileport.hpp>

extern "C" int fileport_makeport(int, mach_port_t *);
extern "C" int fileport_makefd(mach_port_t port);

namespace mach {

mcom::Result<SendRight> MakeFileport(const mcom::File &file) {
  mach_port_name_t name = 0;
  int status = fileport_makeport(file.Descriptor(), &name);
  if (status != 0) {
    return std::error_code{status, error_category()};
  }
  return SendRight::Construct(name);
}

mcom::Result<mcom::File> OpenFileport(SendRight fileport) {
  int fd = fileport_makefd(fileport.Name());
  if (fd < 0) {
    return std::error_code{errno, std::system_category()};
  }
  return mcom::File::WithDescriptor(fd);
}

mach_msg_port_descriptor_t message_codable<mcom::File>::pack(
    const mcom::File &file) {
  return message_codable<Move<SendRight>>{}.pack(
      {MakeFileport(file).ValueOr(SendRight::Construct(0))});
}

mcom::File message_codable<mcom::File>::unpack(
    mach_msg_port_descriptor_t &descriptor) {
  return OpenFileport(message_codable<SendRight>{}.unpack(descriptor))
      .ValueOr(mcom::File::WithDescriptor(-1));
}

}  // namespace mach
