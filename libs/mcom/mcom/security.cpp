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

#include "mcom/security.hpp"

#include <Security/Security.h>
#include <os/log.h>

#include <mcom/dispatch.hpp>
#include <mcom/result.hpp>
#include <mcom/string.hpp>

namespace mcom {

namespace {

SecCodeRef CopyGuest(const AuditToken &audit) {
  const auto pid = audit.Pid();

  auto attributes =
      cf::Dictionary::Create({{kSecGuestAttributePid, cf::Number{pid}.get()}});

  SecCodeRef code = nullptr;

  auto status = SecCodeCopyGuestWithAttributes(nullptr, attributes.get(),
                                               kSecCSDefaultFlags, &code);
  if (status) {
    std::error_code err{status, security_category()};
    os_log_error(OS_LOG_DEFAULT, "failed to copy guest for pid %d: %{public}s",
                 audit.Pid(), err.message().c_str());
    MCOM_CHECK(err);
  }

  return code;
}

class security_category_impl : public std::error_category {
  const char *name() const noexcept override { return "security"; }

  std::string message(int cnd) const override {
    return cf::String{SecCopyErrorMessageString(cnd, nullptr)}.GetCString();
  }
};

SecRequirementRef CreateRequirement(const std::string &req_str) {
  SecRequirementRef requirement;
  CFErrorRef error = nullptr;
  int status = SecRequirementCreateWithStringAndErrors(
      cf::String::WithCString(req_str.c_str()).get(), kSecCSDefaultFlags,
      &error, &requirement);
  if (status) {
    os_log_error(
        OS_LOG_DEFAULT,
        "failed to create requirement from string '%{public}s': %{public}@",
        req_str.c_str(), error);
    cf::ReleaseSafe(error);
    throw std::system_error{status, security_category()};
  }
  return requirement;
}

SecStaticCodeRef CreateStaticCode(const FilePath &path) {
  SecStaticCodeRef code = nullptr;
  auto status = SecStaticCodeCreateWithPath(cf::URL{path, false}.get(),
                                            kSecCSDefaultFlags, &code);
  if (status) {
    os_log_error(OS_LOG_DEFAULT,
                 "failed to create static code from %{public}s: %{public}s",
                 path.CString(), security_category().message(status).c_str());
    throw std::system_error{status, security_category()};
  }
  return code;
}

const char *CodeSigningExtensionString(CodeSigningExtension extension) {
  switch (extension) {
    case CodeSigningExtension::MacDeveloper:
      return "1.2.840.113635.100.6.1.12";

    case CodeSigningExtension::DeveloperIDApplication:
      return "1.2.840.113635.100.6.1.13";
  }
}

Requirement SignedAppRequirement(const std::optional<std::string> &identifier,
                                 const std::string &team_identifier,
                                 CodeSigningExtension extension) {
  const auto identifier_req =
      identifier ? StrFormat("and identifier \"%s\"", identifier->c_str()) : "";

  return Requirement{
      StrFormat("anchor apple generic and anchor trusted %s and "
                "certificate leaf [subject.OU] = \"%s\" and "
                "certificate leaf [field.%s]",
                identifier_req.c_str(), team_identifier.c_str(),
                CodeSigningExtensionString(extension))};
}

Result<CodeSigningExtension> CurrentSigningModeImpl() {
  auto code = Code::Self();

  try {
    code.ValidateSignedApp(std::nullopt, "LSJ6YVK468",
                           CodeSigningExtension::DeveloperIDApplication);
    return CodeSigningExtension::DeveloperIDApplication;
  } catch (const std::system_error &) {
  }

  try {
    code.ValidateSignedApp(std::nullopt, "LSJ6YVK468",
                           CodeSigningExtension::MacDeveloper);
    return CodeSigningExtension::MacDeveloper;
  } catch (const std::system_error &error) {
    return error.code();
  }
}

SigningInformation GetSigningInformationImpl(SecStaticCodeRef code) {
  CFDictionaryRef dict_ref = nullptr;

  auto status =
      SecCodeCopySigningInformation(code, kSecCSSigningInformation, &dict_ref);
  if (status) {
    std::error_code err{status, security_category()};
    os_log_error(OS_LOG_DEFAULT,
                 "failed to copy signing information: %{public}s",
                 err.message().c_str());
    MCOM_CHECK(err);
  }

  cf::Dictionary info_dict = dict_ref;
  auto path = info_dict.GetValue<cf::URL>(kSecCodeInfoMainExecutable)
                  .FileSystemRepresentation(true);
  if (!path) {
    std::error_code err{errSecCSGuestInvalid, security_category()};
    os_log_error(OS_LOG_DEFAULT, "no executable url in guest info: %{public}s",
                 err.message().c_str());
    MCOM_CHECK(err);
  }

  auto identifier = info_dict.GetValue<cf::String>(kSecCodeInfoIdentifier);
  if (!identifier) {
    std::error_code err{errSecCSGuestInvalid, security_category()};
    os_log_error(OS_LOG_DEFAULT, "no identifier in guest info: %{public}s",
                 err.message().c_str());
    MCOM_CHECK(err);
  }

  auto team_identifier =
      info_dict.GetValue<cf::String>(kSecCodeInfoTeamIdentifier);
  if (!team_identifier) {
    std::error_code err{errSecCSGuestInvalid, security_category()};
    os_log_error(OS_LOG_DEFAULT, "no team identifier in guest info: %{public}s",
                 err.message().c_str());
    MCOM_CHECK(err);
  }

  cf::Dictionary info_plist =
      info_dict.GetValue<cf::Dictionary>(kSecCodeInfoPList);
  if (!info_plist) {
    std::error_code err{errSecCSGuestInvalid, security_category()};
    os_log_error(OS_LOG_DEFAULT, "no info plist in guest info: %{public}s",
                 err.message().c_str());
    MCOM_CHECK(err);
  }

  std::optional<SigningInformation::TimePoint> timestamp =
      [&]() -> std::optional<SigningInformation::TimePoint> {
    cf::Date timestamp = info_dict.GetValue<cf::Date>(kSecCodeInfoTimestamp);
    if (!timestamp) {
      return std::nullopt;
    }

    return SigningInformation::TimePoint{
        std::chrono::round<SigningInformation::Clock::duration>(
            std::chrono::duration<CFTimeInterval>(
                timestamp.AbsoluteTimeSince1970()))};
  }();

  std::array<uint8_t, 20> cdhash{0};
  cf::Data cdhash_data = info_dict.GetValue<cf::Data>(kSecCodeInfoUnique);
  if (cdhash_data && cdhash_data.GetLength() == 20) {
    cdhash_data.GetBytes(CFRangeMake(0, 20), cdhash.data());
  }

  return SigningInformation{identifier.GetCString(),
                            std::move(*path),
                            team_identifier.GetCString(),
                            info_plist,
                            timestamp,
                            cdhash};
}

}  // namespace

const std::error_category &security_category() {
  static security_category_impl cat;
  return cat;
}

Requirement::Requirement(const std::string &req_str)
    : Ptr{CreateRequirement(req_str)} {}

Code::Code(cf::Ptr<SecCodeRef> ptr) : Ptr{std::move(ptr)} {}

Code::Code(const AuditToken &audit) : Ptr{CopyGuest(audit)} {}

Code Code::Self() {
  SecCodeRef code_ref = nullptr;
  auto status = SecCodeCopySelf(kSecCSDefaultFlags, &code_ref);
  if (status) {
    std::error_code err{status, security_category()};
    os_log_error(OS_LOG_DEFAULT, "code copy self failed: %{public}s",
                 err.message().c_str());
    MCOM_CHECK(err);
  }
  return Code{Ptr{code_ref}};
}

void Code::CheckValidity(const std::optional<Requirement> &requirement) {
  CFErrorRef error = nullptr;
  auto req_ref = requirement ? requirement->get() : nullptr;
  auto status =
      SecCodeCheckValidityWithErrors(cf_, kSecCSDefaultFlags, req_ref, &error);
  if (status) {
    os_log_error(OS_LOG_DEFAULT, "code validation failed: %{public}@", error);
    cf::ReleaseSafe(error);
    throw std::system_error{status, security_category()};
  }
}

void Code::ValidateSignedApp(const std::optional<std::string> &identifier,
                             const std::string &team_identifier,
                             CodeSigningExtension extension) {
  CheckValidity(SignedAppRequirement(identifier, team_identifier, extension));
}

SigningInformation Code::GetSigningInformation() {
  CheckValidity();

  return GetSigningInformationImpl(cf_);
}

StaticCode::StaticCode(const FilePath &path) : Ptr{CreateStaticCode(path)} {}

void StaticCode::CheckValidity(const std::optional<Requirement> &requirement) {
  CFErrorRef error = nullptr;
  SecCSFlags flags = kSecCSCheckNestedCode | kSecCSStrictValidate;
  auto req_ref = requirement ? requirement->get() : nullptr;
  auto status =
      SecStaticCodeCheckValidityWithErrors(cf_, flags, req_ref, &error);
  if (status) {
    os_log_error(OS_LOG_DEFAULT, "code validation failed: %{public}@", error);
    cf::ReleaseSafe(error);
    throw std::system_error{status, security_category()};
  }
}

void StaticCode::ValidateSignedApp(const std::optional<std::string> &identifier,
                                   const std::string &team_identifier,
                                   CodeSigningExtension extension) {
  CheckValidity(SignedAppRequirement(identifier, team_identifier, extension));
}

SigningInformation StaticCode::GetSigningInformation() {
  CheckValidity();

  return GetSigningInformationImpl(cf_);
}

CodeSigningExtension CurrentSigningMode() {
  const auto &mode = MCOM_ONCE(CurrentSigningModeImpl);

  if (!mode) {
    os_log_error(OS_LOG_DEFAULT, "invalid signature: %s\n",
                 mode.Code().message().c_str());
    std::exit(1);
  }

  return *mode;
}

}  // namespace mcom
