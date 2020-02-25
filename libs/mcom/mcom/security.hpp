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

#include <Security/SecCode.h>
#include <bsm/libbsm.h>
#include <mach/message.h>
#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <system_error>

#include <mcom/cf.hpp>
#include <mcom/file_path.hpp>

namespace mcom {

const std::error_category &security_category();

enum class CodeSigningExtension { MacDeveloper, DeveloperIDApplication };

struct SigningInformation {
  using Clock = std::chrono::system_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  std::string identifier;
  FilePath main_executable;
  std::string team_identifier;
  cf::Dictionary info_plist;
  std::optional<TimePoint> timestamp;
  std::array<uint8_t, 20> cdhash;
};

class Requirement : public cf::Ptr<SecRequirementRef> {
 public:
  Requirement(const std::string &str);
};

class AuditToken {
 public:
  constexpr AuditToken(const audit_token_t &token) : token_{token} {}

  const pid_t Pid() const { return audit_token_to_pid(token_); }

  const uid_t EUid() const { return audit_token_to_euid(token_); }

 private:
  const audit_token_t token_{};
};

class Code : public cf::Ptr<SecCodeRef> {
 public:
  Code(const AuditToken &audit);

  static Code Self();

  void CheckValidity(
      const std::optional<Requirement> &requirement = std::nullopt);

  void ValidateSignedApp(const std::optional<std::string> &identifier,
                         const std::string &team_identifier,
                         CodeSigningExtension extension);

  SigningInformation GetSigningInformation();

 private:
  Code(cf::Ptr<SecCodeRef>);
};

class StaticCode : public cf::Ptr<SecStaticCodeRef> {
 public:
  StaticCode(const FilePath &path);

  static StaticCode WithBundleAtPath(const FilePath &path);

  void CheckValidity(
      const std::optional<Requirement> &requirement = std::nullopt);

  void ValidateSignedApp(const std::optional<std::string> &identifier,
                         const std::string &team_identifier,
                         CodeSigningExtension extension);

  SigningInformation GetSigningInformation();
};

CodeSigningExtension CurrentSigningMode();

}  // namespace mcom
