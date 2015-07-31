// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installation_validator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using installer::ChannelInfo;
using installer::InstallationValidator;
using installer::InstallationState;
using installer::AppCommand;
using installer::ProductState;
using testing::_;
using testing::StrictMock;
using testing::Values;

namespace {

enum Channel {
  STABLE_CHANNEL,
  BETA_CHANNEL,
  DEV_CHANNEL
};

enum PackageType {
  SINGLE_INSTALL,
  MULTI_INSTALL
};

enum Level {
  USER_LEVEL,
  SYSTEM_LEVEL
};

enum Vehicle {
  GOOGLE_UPDATE,
  MSI
};

enum ChannelModifier {
  CM_MULTI        = 0x01,
  CM_CHROME       = 0x02,
  CM_CHROME_FRAME = 0x04,
  CM_FULL         = 0x08
};

const wchar_t* const kChromeChannels[] = {
  L"",
  L"1.1-beta",
  L"2.0-dev"
};

const wchar_t* const kChromeFrameChannels[] = {
  L"",
  L"beta",
  L"dev"
};

class FakeProductState : public ProductState {
 public:
  void SetChannel(const wchar_t* base, int channel_modifiers);
  void SetVersion(const char* version);
  void SetUninstallCommand(BrowserDistribution::Type dist_type,
                           Level install_level,
                           const char* version,
                           int channel_modifiers,
                           Vehicle vehicle);
  void AddOsUpgradeCommand(BrowserDistribution::Type dist_type,
                           Level install_level,
                           const char* version,
                           int channel_modifiers);
  void set_multi_install(bool is_multi_install) {
    multi_install_ = is_multi_install;
  }
  installer::AppCommands& commands() { return commands_; }

 protected:
  struct ChannelMethodForModifier {
    ChannelModifier modifier;
    bool (ChannelInfo::*method)(bool value);
  };

  static base::FilePath GetSetupPath(
      BrowserDistribution::Type dist_type,
      Level install_level,
      int channel_modifiers);

  static base::FilePath GetSetupExePath(
      BrowserDistribution::Type dist_type,
      Level install_level,
      const char* version,
      int channel_modifiers);

  static const ChannelMethodForModifier kChannelMethods[];
};

class FakeInstallationState : public InstallationState {
 public:
  void SetProductState(BrowserDistribution::Type type,
                       Level install_level,
                       const ProductState& product) {
    GetProducts(install_level)[IndexFromDistType(type)].CopyFrom(product);
  }

 protected:
  ProductState* GetProducts(Level install_level) {
    return install_level == USER_LEVEL ? user_products_ : system_products_;
  }
};

// static
const FakeProductState::ChannelMethodForModifier
    FakeProductState::kChannelMethods[] = {
  { CM_MULTI,        &ChannelInfo::SetMultiInstall },
  { CM_CHROME,       &ChannelInfo::SetChrome },
  { CM_CHROME_FRAME, &ChannelInfo::SetChromeFrame },
  { CM_FULL,         &ChannelInfo::SetFullSuffix }
};

// static
base::FilePath FakeProductState::GetSetupPath(
    BrowserDistribution::Type dist_type,
    Level install_level,
    int channel_modifiers) {
  const bool is_multi_install = (channel_modifiers & CM_MULTI) != 0;
  return installer::GetChromeInstallPath(
      install_level == SYSTEM_LEVEL,
      BrowserDistribution::GetSpecificDistribution(is_multi_install ?
              BrowserDistribution::CHROME_BINARIES : dist_type));
}

// static
base::FilePath FakeProductState::GetSetupExePath(
    BrowserDistribution::Type dist_type,
    Level install_level,
    const char* version,
    int channel_modifiers) {
  base::FilePath setup_path = GetSetupPath(dist_type, install_level,
                                           channel_modifiers);
  return setup_path
      .AppendASCII(version)
      .Append(installer::kInstallerDir)
      .Append(installer::kSetupExe);
}

// Sets the channel_ member of this instance according to a base channel value
// and a set of modifiers.
void FakeProductState::SetChannel(const wchar_t* base, int channel_modifiers) {
  channel_.set_value(base);
  for (size_t i = 0; i < arraysize(kChannelMethods); ++i) {
    if ((channel_modifiers & kChannelMethods[i].modifier) != 0)
      (channel_.*kChannelMethods[i].method)(true);
  }
}

void FakeProductState::SetVersion(const char* version) {
  version_.reset(version == NULL ? NULL : new Version(version));
}

// Sets the uninstall command for this object.
void FakeProductState::SetUninstallCommand(BrowserDistribution::Type dist_type,
                                           Level install_level,
                                           const char* version,
                                           int channel_modifiers,
                                           Vehicle vehicle) {
  DCHECK(version);

  const bool is_multi_install = (channel_modifiers & CM_MULTI) != 0;
  uninstall_command_ = base::CommandLine(
      GetSetupExePath(dist_type, install_level, version, channel_modifiers));
  uninstall_command_.AppendSwitch(installer::switches::kUninstall);
  if (install_level == SYSTEM_LEVEL)
    uninstall_command_.AppendSwitch(installer::switches::kSystemLevel);
  if (is_multi_install) {
    uninstall_command_.AppendSwitch(installer::switches::kMultiInstall);
    if (dist_type == BrowserDistribution::CHROME_BROWSER)
      uninstall_command_.AppendSwitch(installer::switches::kChrome);
    else if (dist_type == BrowserDistribution::CHROME_FRAME)
      uninstall_command_.AppendSwitch(installer::switches::kChromeFrame);
  } else if (dist_type == BrowserDistribution::CHROME_FRAME) {
    uninstall_command_.AppendSwitch(installer::switches::kChromeFrame);
  }
  if (vehicle == MSI)
    uninstall_command_.AppendSwitch(installer::switches::kMsi);
}

// Adds the "on-os-upgrade" Google Update product command.
void FakeProductState::AddOsUpgradeCommand(BrowserDistribution::Type dist_type,
                                           Level install_level,
                                           const char* version,
                                           int channel_modifiers) {
  // Right now only Chrome browser uses this.
  DCHECK_EQ(dist_type, BrowserDistribution::CHROME_BROWSER);

  base::CommandLine cmd_line(
      GetSetupExePath(dist_type, install_level, version, channel_modifiers));
  cmd_line.AppendSwitch(installer::switches::kOnOsUpgrade);
  // Imitating ChromeBrowserOperations::AppendProductFlags().
  if ((channel_modifiers & CM_MULTI) != 0) {
    cmd_line.AppendSwitch(installer::switches::kMultiInstall);
    cmd_line.AppendSwitch(installer::switches::kChrome);
  }
  if (install_level == SYSTEM_LEVEL)
    cmd_line.AppendSwitch(installer::switches::kSystemLevel);
  cmd_line.AppendSwitch(installer::switches::kVerboseLogging);
  AppCommand app_cmd(cmd_line.GetCommandLineString());
  app_cmd.set_is_auto_run_on_os_upgrade(true);
  commands_.Set(installer::kCmdOnOsUpgrade, app_cmd);
}

}  // namespace

// Fixture for testing the InstallationValidator.  Errors logged by the
// validator are sent to an optional mock recipient (see
// set_validation_error_recipient) upon which expectations can be placed.
class InstallationValidatorTest
    : public testing::TestWithParam<InstallationValidator::InstallationType> {
 public:

  // These shouldn't need to be public, but there seems to be some interaction
  // with parameterized tests that requires it.
  static void SetUpTestCase();
  static void TearDownTestCase();

  // Returns the multi channel modifiers for a given installation type.
  static int GetChannelModifiers(InstallationValidator::InstallationType type);

 protected:
  typedef std::map<InstallationValidator::InstallationType, int>
      InstallationTypeToModifiers;

  class ValidationErrorRecipient {
   public:
    virtual ~ValidationErrorRecipient() { }
    virtual void ReceiveValidationError(const char* file,
                                        int line,
                                        const char* message) = 0;
  };
  class MockValidationErrorRecipient : public ValidationErrorRecipient {
   public:
    MOCK_METHOD3(ReceiveValidationError, void(const char* file,
                                              int line,
                                              const char* message));
  };

 protected:
  static bool HandleLogMessage(int severity,
                               const char* file,
                               int line,
                               size_t message_start,
                               const std::string& str);
  static void set_validation_error_recipient(
      ValidationErrorRecipient* recipient);
  static void MakeProductState(
      BrowserDistribution::Type prod_type,
      InstallationValidator::InstallationType inst_type,
      Level install_level,
      Channel channel,
      Vehicle vehicle,
      FakeProductState* state);
  static void MakeMachineState(
      InstallationValidator::InstallationType inst_type,
      Level install_level,
      Channel channel,
      Vehicle vehicle,
      FakeInstallationState* state);
  void TearDown() override;

  static logging::LogMessageHandlerFunction old_log_message_handler_;
  static ValidationErrorRecipient* validation_error_recipient_;
  static InstallationTypeToModifiers* type_to_modifiers_;
};

// static
logging::LogMessageHandlerFunction
    InstallationValidatorTest::old_log_message_handler_ = NULL;

// static
InstallationValidatorTest::ValidationErrorRecipient*
    InstallationValidatorTest::validation_error_recipient_ = NULL;

// static
InstallationValidatorTest::InstallationTypeToModifiers*
    InstallationValidatorTest::type_to_modifiers_ = NULL;

// static
int InstallationValidatorTest::GetChannelModifiers(
    InstallationValidator::InstallationType type) {
  DCHECK(type_to_modifiers_);
  DCHECK(type_to_modifiers_->find(type) != type_to_modifiers_->end());

  return (*type_to_modifiers_)[type];
}

// static
void InstallationValidatorTest::SetUpTestCase() {
  DCHECK(type_to_modifiers_ == NULL);
  old_log_message_handler_ = logging::GetLogMessageHandler();
  logging::SetLogMessageHandler(&HandleLogMessage);

  type_to_modifiers_ = new InstallationTypeToModifiers();
  InstallationTypeToModifiers& ttm = *type_to_modifiers_;
  ttm[InstallationValidator::NO_PRODUCTS] = 0;
  ttm[InstallationValidator::CHROME_SINGLE] = 0;
  ttm[InstallationValidator::CHROME_MULTI] = CM_MULTI | CM_CHROME;
  ttm[InstallationValidator::CHROME_FRAME_SINGLE] = 0;
  ttm[InstallationValidator::CHROME_FRAME_SINGLE_CHROME_SINGLE] = 0;
  ttm[InstallationValidator::CHROME_FRAME_SINGLE_CHROME_MULTI] =
      CM_MULTI | CM_CHROME;
  ttm[InstallationValidator::CHROME_FRAME_MULTI] = CM_MULTI | CM_CHROME_FRAME;
  ttm[InstallationValidator::CHROME_FRAME_MULTI_CHROME_MULTI] =
      CM_MULTI | CM_CHROME_FRAME | CM_CHROME;
}

// static
void InstallationValidatorTest::TearDownTestCase() {
  logging::SetLogMessageHandler(old_log_message_handler_);
  old_log_message_handler_ = NULL;

  delete type_to_modifiers_;
  type_to_modifiers_ = NULL;
}

// static
bool InstallationValidatorTest::HandleLogMessage(int severity,
                                                 const char* file,
                                                 int line,
                                                 size_t message_start,
                                                 const std::string& str) {
  // All validation failures result in LOG(ERROR)
  if (severity == logging::LOG_ERROR && !str.empty()) {
    // Remove the trailing newline, if present.
    size_t message_length = str.size() - message_start;
    if (*str.rbegin() == '\n')
      --message_length;
    if (validation_error_recipient_ != NULL) {
      validation_error_recipient_->ReceiveValidationError(
          file, line, str.substr(message_start, message_length).c_str());
    } else {
      // Fail the test if an error wasn't handled.
      ADD_FAILURE_AT(file, line)
          << base::StringPiece(str.c_str() + message_start, message_length);
    }
    return true;
  }

  if (old_log_message_handler_ != NULL)
    return (old_log_message_handler_)(severity, file, line, message_start, str);

  return false;
}

// static
void InstallationValidatorTest::set_validation_error_recipient(
    ValidationErrorRecipient* recipient) {
  validation_error_recipient_ = recipient;
}

// static
// Populates |state| with the state of a valid installation of product
// |prod_type|.  |inst_type| dictates properties of the installation
// (multi-install, etc).
void InstallationValidatorTest::MakeProductState(
    BrowserDistribution::Type prod_type,
    InstallationValidator::InstallationType inst_type,
    Level install_level,
    Channel channel,
    Vehicle vehicle,
    FakeProductState* state) {
  DCHECK(state);

  const bool is_multi_install =
      prod_type == BrowserDistribution::CHROME_BINARIES ||
      (prod_type == BrowserDistribution::CHROME_BROWSER &&
       (inst_type & InstallationValidator::ProductBits::CHROME_MULTI) != 0) ||
      (prod_type == BrowserDistribution::CHROME_FRAME &&
       (inst_type &
           InstallationValidator::ProductBits::CHROME_FRAME_MULTI) != 0);

  const wchar_t* const* channels = &kChromeChannels[0];
  if (prod_type == BrowserDistribution::CHROME_FRAME && !is_multi_install)
    channels = &kChromeFrameChannels[0];  // SxS GCF has its own channel names.
  const int channel_modifiers =
      is_multi_install ? GetChannelModifiers(inst_type) : 0;

  state->Clear();
  state->SetChannel(channels[channel], channel_modifiers);
  state->SetVersion(chrome::kChromeVersion);
  state->SetUninstallCommand(prod_type, install_level, chrome::kChromeVersion,
                             channel_modifiers, vehicle);
  state->set_multi_install(is_multi_install);
  if (prod_type == BrowserDistribution::CHROME_BROWSER) {
    state->AddOsUpgradeCommand(prod_type,
                               install_level,
                               chrome::kChromeVersion,
                               channel_modifiers);
  }
}

// static
// Populates |state| with the state of a valid installation of |inst_type|.
void InstallationValidatorTest::MakeMachineState(
    InstallationValidator::InstallationType inst_type,
    Level install_level,
    Channel channel,
    Vehicle vehicle,
    FakeInstallationState* state) {
  DCHECK(state);

  static const int kChromeMask =
      (InstallationValidator::ProductBits::CHROME_SINGLE |
       InstallationValidator::ProductBits::CHROME_MULTI);
  static const int kChromeFrameMask =
      (InstallationValidator::ProductBits::CHROME_FRAME_SINGLE |
       InstallationValidator::ProductBits::CHROME_FRAME_MULTI);
  static const int kBinariesMask =
      (InstallationValidator::ProductBits::CHROME_MULTI |
       InstallationValidator::ProductBits::CHROME_FRAME_MULTI);

  FakeProductState prod_state;

  if ((inst_type & kChromeMask) != 0) {
    MakeProductState(BrowserDistribution::CHROME_BROWSER, inst_type,
                     install_level, channel, vehicle, &prod_state);
    state->SetProductState(BrowserDistribution::CHROME_BROWSER, install_level,
                           prod_state);
  }

  if ((inst_type & kChromeFrameMask) != 0) {
    MakeProductState(BrowserDistribution::CHROME_FRAME, inst_type,
                     install_level, channel, vehicle, &prod_state);
    state->SetProductState(BrowserDistribution::CHROME_FRAME, install_level,
                           prod_state);
  }

  if ((inst_type & kBinariesMask) != 0) {
    MakeProductState(BrowserDistribution::CHROME_BINARIES, inst_type,
                     install_level, channel, vehicle, &prod_state);
    state->SetProductState(BrowserDistribution::CHROME_BINARIES, install_level,
                           prod_state);
  }
}

void InstallationValidatorTest::TearDown() {
  validation_error_recipient_ = NULL;
}

// Builds a proper machine state for a given InstallationType, then validates
// it.
TEST_P(InstallationValidatorTest, TestValidInstallation) {
  const InstallationValidator::InstallationType inst_type = GetParam();
  FakeInstallationState machine_state;
  InstallationValidator::InstallationType type;
  StrictMock<MockValidationErrorRecipient> recipient;
  set_validation_error_recipient(&recipient);

  MakeMachineState(inst_type, SYSTEM_LEVEL, STABLE_CHANNEL, GOOGLE_UPDATE,
                   &machine_state);
  EXPECT_TRUE(InstallationValidator::ValidateInstallationTypeForState(
                  machine_state, true, &type));
  EXPECT_EQ(inst_type, type);
}

// Run the test for all installation types.
INSTANTIATE_TEST_CASE_P(
    AllValidInstallations,
    InstallationValidatorTest,
    Values(InstallationValidator::NO_PRODUCTS,
           InstallationValidator::CHROME_SINGLE,
           InstallationValidator::CHROME_MULTI,
           InstallationValidator::CHROME_FRAME_SINGLE,
           InstallationValidator::CHROME_FRAME_SINGLE_CHROME_SINGLE,
           InstallationValidator::CHROME_FRAME_SINGLE_CHROME_MULTI,
           InstallationValidator::CHROME_FRAME_MULTI,
           InstallationValidator::CHROME_FRAME_MULTI_CHROME_MULTI));
