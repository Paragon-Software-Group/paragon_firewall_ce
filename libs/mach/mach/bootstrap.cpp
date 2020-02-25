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
// Created by Alexey Antipov on 13/02/2019.
//

#include "bootstrap.hpp"

#include <bootstrap.h>

namespace mach {

mcom::Result<ReceiveRight> BootstrapCheckIn(const char *service_name) {
  mach_port_name_t port;
  const auto status =
      ::bootstrap_check_in(::bootstrap_port, service_name, &port);
  if (status != KERN_SUCCESS) {
    return std::error_code{status, std::system_category()};
  }
  return ReceiveRight::Construct(port);
}

mcom::Result<SendRight> BootstrapLookUp(const char *service_name) {
  mach_port_name_t port;
  const auto status =
      ::bootstrap_look_up(::bootstrap_port, service_name, &port);
  if (status != KERN_SUCCESS) {
    return std::error_code{status, std::system_category()};
  }
  return SendRight::Construct(port);
}

}  // namespace mach
