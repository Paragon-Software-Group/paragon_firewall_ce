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
// Created by Alexey Antipov on 28/03/2019.
//

#include "message.hpp"

#include <string>

namespace {

class error_category_impl : public std::error_category {
  const char *name() const noexcept override { return "mach"; }
  std::string message(int cnd) const override { return mach_error_string(cnd); }
};

}  // namespace

namespace mach {

const std::error_category &error_category() noexcept {
  static const error_category_impl ecat;
  return ecat;
}

mcom::Result<MessageBuffer> MessageBuffer::Receive(const ReceiveRight &right,
                                                   size_t size) {
  const auto buffer_size = size + sizeof(mach_msg_audit_trailer_t);

  Buffer buffer{std::malloc(buffer_size), std::free};

  auto header_ptr = static_cast<mach_msg_header_t *>(buffer.get());

  const auto status = ::mach_msg(
      header_ptr,
      MACH_RCV_MSG | MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT), 0,
      static_cast<mach_msg_size_t>(buffer_size), right.Name(),
      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  if (status != 0) {
    return std::error_code{status, error_category()};
  }

  header_ptr->msgh_local_port = MACH_PORT_NULL;

  return {std::move(buffer)};
}

MessageBuffer::MessageBuffer(Buffer buffer) : buffer_{std::move(buffer)} {}

MessageBuffer::~MessageBuffer() {
  if (buffer_.get()) {
    mach_msg_destroy(&Header());
  }
}

mach_msg_header_t &MessageBuffer::Header() {
  return *static_cast<mach_msg_header_t *>(buffer_.get());
}

const mach_msg_header_t &MessageBuffer::Header() const {
  return *static_cast<const mach_msg_header_t *>(buffer_.get());
}

mach_msg_id_t MessageBuffer::MessageId() const { return Header().msgh_id; }

mcom::AuditToken MessageBuffer::AuditToken() const {
  auto trailer_base =
      static_cast<const uint8_t *>(buffer_.get()) + Header().msgh_size;
  auto trailer_ptr =
      reinterpret_cast<const mach_msg_audit_trailer_t *>(trailer_base);
  return {trailer_ptr->msgh_audit};
}

mcom::Optional<SendOnceRight> MessageBuffer::ExtractReplyPort() {
  auto &header = Header();
  if (MACH_MSGH_BITS_REMOTE(header.msgh_bits) == MACH_MSG_TYPE_MOVE_SEND_ONCE &&
      header.msgh_remote_port != MACH_PORT_NULL) {
    auto name = header.msgh_remote_port;
    header.msgh_remote_port = MACH_PORT_NULL;
    return SendOnceRight::Construct(name);
  }
  return mcom::nullopt;
}

}  // namespace mach
