// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/configuration_policy_handler_chromeos.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_value_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "crypto/sha2.h"
#include "grit/components_strings.h"
#include "policy/policy_constants.h"
#include "ui/chromeos/accessibility_types.h"
#include "url/gurl.h"

namespace policy {

namespace {

const char kSubkeyURL[] = "url";
const char kSubkeyHash[] = "hash";

bool GetSubkeyString(const base::DictionaryValue& dict,
                     policy::PolicyErrorMap* errors,
                     const std::string& policy,
                     const std::string& subkey,
                     std::string* value) {
  const base::Value* raw_value = NULL;
  if (!dict.GetWithoutPathExpansion(subkey, &raw_value)) {
    errors->AddError(policy, subkey, IDS_POLICY_NOT_SPECIFIED_ERROR);
    return false;
  }
  std::string string_value;
  if (!raw_value->GetAsString(&string_value)) {
    errors->AddError(policy, subkey, IDS_POLICY_TYPE_ERROR, "string");
    return false;
  }
  if (string_value.empty()) {
    errors->AddError(policy, subkey, IDS_POLICY_NOT_SPECIFIED_ERROR);
    return false;
  }
  *value = string_value;
  return true;
}

const char kScreenDimDelayAC[] = "AC.Delays.ScreenDim";
const char kScreenOffDelayAC[] = "AC.Delays.ScreenOff";
const char kIdleWarningDelayAC[] = "AC.Delays.IdleWarning";
const char kIdleDelayAC[] = "AC.Delays.Idle";
const char kIdleActionAC[] = "AC.IdleAction";

const char kScreenDimDelayBattery[] = "Battery.Delays.ScreenDim";
const char kScreenOffDelayBattery[] = "Battery.Delays.ScreenOff";
const char kIdleWarningDelayBattery[] = "Battery.Delays.IdleWarning";
const char kIdleDelayBattery[] = "Battery.Delays.Idle";
const char kIdleActionBattery[] = "Battery.IdleAction";

const char kScreenLockDelayAC[] = "AC";
const char kScreenLockDelayBattery[] = "Battery";

const char kActionSuspend[] = "Suspend";
const char kActionLogout[] = "Logout";
const char kActionShutdown[]  = "Shutdown";
const char kActionDoNothing[] = "DoNothing";

scoped_ptr<base::Value> GetValue(const base::DictionaryValue* dict,
                                 const char* key) {
  const base::Value* value = NULL;
  if (!dict->Get(key, &value))
    return scoped_ptr<base::Value>();
  return value->CreateDeepCopy();
}

scoped_ptr<base::Value> GetAction(const base::DictionaryValue* dict,
                                  const char* key) {
  scoped_ptr<base::Value> value = GetValue(dict, key);
  std::string action;
  if (!value || !value->GetAsString(&action))
    return scoped_ptr<base::Value>();
  if (action == kActionSuspend) {
    return scoped_ptr<base::Value>(new base::FundamentalValue(
        chromeos::PowerPolicyController::ACTION_SUSPEND));
  }
  if (action == kActionLogout) {
    return scoped_ptr<base::Value>(new base::FundamentalValue(
        chromeos::PowerPolicyController::ACTION_STOP_SESSION));
  }
  if (action == kActionShutdown) {
    return scoped_ptr<base::Value>(new base::FundamentalValue(
        chromeos::PowerPolicyController::ACTION_SHUT_DOWN));
  }
  if (action == kActionDoNothing) {
    return scoped_ptr<base::Value>(new base::FundamentalValue(
        chromeos::PowerPolicyController::ACTION_DO_NOTHING));
  }
  return scoped_ptr<base::Value>();
}

}  // namespace

ExternalDataPolicyHandler::ExternalDataPolicyHandler(const char* policy_name)
    : TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_DICTIONARY) {
}

ExternalDataPolicyHandler::~ExternalDataPolicyHandler() {
}

bool ExternalDataPolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                    PolicyErrorMap* errors) {
  if (!TypeCheckingPolicyHandler::CheckPolicySettings(policies, errors))
    return false;

  const std::string policy = policy_name();
  const base::Value* value = policies.GetValue(policy);
  if (!value)
    return true;

  const base::DictionaryValue* dict = NULL;
  value->GetAsDictionary(&dict);
  if (!dict) {
    NOTREACHED();
    return false;
  }
  std::string url_string;
  std::string hash_string;
  if (!GetSubkeyString(*dict, errors, policy, kSubkeyURL, &url_string) ||
      !GetSubkeyString(*dict, errors, policy, kSubkeyHash, &hash_string)) {
    return false;
  }

  const GURL url(url_string);
  if (!url.is_valid()) {
    errors->AddError(policy, kSubkeyURL, IDS_POLICY_VALUE_FORMAT_ERROR);
    return false;
  }

  std::vector<uint8> hash;
  if (!base::HexStringToBytes(hash_string, &hash) ||
      hash.size() != crypto::kSHA256Length) {
    errors->AddError(policy, kSubkeyHash, IDS_POLICY_VALUE_FORMAT_ERROR);
    return false;
  }

  return true;
}

void ExternalDataPolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                    PrefValueMap* prefs) {
}

// static
NetworkConfigurationPolicyHandler*
NetworkConfigurationPolicyHandler::CreateForUserPolicy() {
  return new NetworkConfigurationPolicyHandler(
      key::kOpenNetworkConfiguration,
      onc::ONC_SOURCE_USER_POLICY,
      prefs::kOpenNetworkConfiguration);
}

// static
NetworkConfigurationPolicyHandler*
NetworkConfigurationPolicyHandler::CreateForDevicePolicy() {
  return new NetworkConfigurationPolicyHandler(
      key::kDeviceOpenNetworkConfiguration,
      onc::ONC_SOURCE_DEVICE_POLICY,
      prefs::kDeviceOpenNetworkConfiguration);
}

NetworkConfigurationPolicyHandler::~NetworkConfigurationPolicyHandler() {}

bool NetworkConfigurationPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (value) {
    std::string onc_blob;
    value->GetAsString(&onc_blob);
    scoped_ptr<base::DictionaryValue> root_dict =
        chromeos::onc::ReadDictionaryFromJson(onc_blob);
    if (root_dict.get() == NULL) {
      errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_PARSE_FAILED);
      return false;
    }

    // Validate the ONC dictionary. We are liberal and ignore unknown field
    // names and ignore invalid field names in kRecommended arrays.
    chromeos::onc::Validator validator(
        false,  // Ignore unknown fields.
        false,  // Ignore invalid recommended field names.
        true,   // Fail on missing fields.
        true);  // Validate for managed ONC
    validator.SetOncSource(onc_source_);

    // ONC policies are always unencrypted.
    chromeos::onc::Validator::Result validation_result;
    root_dict = validator.ValidateAndRepairObject(
        &chromeos::onc::kToplevelConfigurationSignature, *root_dict,
        &validation_result);
    if (validation_result == chromeos::onc::Validator::VALID_WITH_WARNINGS)
      errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_IMPORT_PARTIAL);
    else if (validation_result == chromeos::onc::Validator::INVALID)
      errors->AddError(policy_name(), IDS_POLICY_NETWORK_CONFIG_IMPORT_FAILED);

    // In any case, don't reject the policy as some networks or certificates
    // could still be applied.
  }

  return true;
}

void NetworkConfigurationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (!value)
    return;

  std::string onc_blob;
  value->GetAsString(&onc_blob);

  scoped_ptr<base::ListValue> network_configs(new base::ListValue);
  base::ListValue certificates;
  base::DictionaryValue global_network_config;
  chromeos::onc::ParseAndValidateOncForImport(onc_blob,
                                              onc_source_,
                                              "",
                                              network_configs.get(),
                                              &global_network_config,
                                              &certificates);

  // Currently, only the per-network configuration is stored in a pref. Ignore
  // |global_network_config| and |certificates|.
  prefs->SetValue(pref_path_, network_configs.Pass());
}

void NetworkConfigurationPolicyHandler::PrepareForDisplaying(
    PolicyMap* policies) const {
  const PolicyMap::Entry* entry = policies->Get(policy_name());
  if (!entry)
    return;
  base::Value* sanitized_config = SanitizeNetworkConfig(entry->value);
  if (!sanitized_config)
    sanitized_config = base::Value::CreateNullValue().release();

  policies->Set(policy_name(), entry->level, entry->scope,
                sanitized_config, NULL);
}

NetworkConfigurationPolicyHandler::NetworkConfigurationPolicyHandler(
    const char* policy_name,
    onc::ONCSource onc_source,
    const char* pref_path)
    : TypeCheckingPolicyHandler(policy_name, base::Value::TYPE_STRING),
      onc_source_(onc_source),
      pref_path_(pref_path) {
}

// static
base::Value* NetworkConfigurationPolicyHandler::SanitizeNetworkConfig(
    const base::Value* config) {
  std::string json_string;
  if (!config->GetAsString(&json_string))
    return NULL;

  scoped_ptr<base::DictionaryValue> toplevel_dict =
      chromeos::onc::ReadDictionaryFromJson(json_string);
  if (!toplevel_dict)
    return NULL;

  // Placeholder to insert in place of the filtered setting.
  const char kPlaceholder[] = "********";

  toplevel_dict = chromeos::onc::MaskCredentialsInOncObject(
      chromeos::onc::kToplevelConfigurationSignature,
      *toplevel_dict,
      kPlaceholder);

  base::JSONWriter::WriteWithOptions(
      *toplevel_dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_string);
  return new base::StringValue(json_string);
}

PinnedLauncherAppsPolicyHandler::PinnedLauncherAppsPolicyHandler()
    : ExtensionListPolicyHandler(key::kPinnedLauncherApps,
                                 prefs::kPinnedLauncherApps,
                                 false) {}

PinnedLauncherAppsPolicyHandler::~PinnedLauncherAppsPolicyHandler() {}

void PinnedLauncherAppsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  PolicyErrorMap errors;
  const base::Value* policy_value = policies.GetValue(policy_name());
  const base::ListValue* policy_list = NULL;
  if (policy_value && policy_value->GetAsList(&policy_list) && policy_list) {
    scoped_ptr<base::ListValue> pinned_apps_list(new base::ListValue());
    for (base::ListValue::const_iterator entry(policy_list->begin());
         entry != policy_list->end(); ++entry) {
      std::string id;
      if ((*entry)->GetAsString(&id)) {
        base::DictionaryValue* app_dict = new base::DictionaryValue();
        app_dict->SetString(ash::kPinnedAppsPrefAppIDPath, id);
        pinned_apps_list->Append(app_dict);
      }
    }
    prefs->SetValue(pref_path(), pinned_apps_list.Pass());
  }
}

ScreenMagnifierPolicyHandler::ScreenMagnifierPolicyHandler()
    : IntRangePolicyHandlerBase(key::kScreenMagnifierType,
                                0, ui::MAGNIFIER_FULL, false) {
}

ScreenMagnifierPolicyHandler::~ScreenMagnifierPolicyHandler() {
}

void ScreenMagnifierPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  int value_in_range;
  if (value && EnsureInRange(value, &value_in_range, NULL)) {
    prefs->SetBoolean(prefs::kAccessibilityScreenMagnifierEnabled,
                      value_in_range != 0);
    prefs->SetInteger(prefs::kAccessibilityScreenMagnifierType, value_in_range);
  }
}

LoginScreenPowerManagementPolicyHandler::
    LoginScreenPowerManagementPolicyHandler(const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(key::kDeviceLoginScreenPowerManagement,
                                    chrome_schema.GetKnownProperty(
                                        key::kDeviceLoginScreenPowerManagement),
                                    SCHEMA_ALLOW_UNKNOWN) {
}

LoginScreenPowerManagementPolicyHandler::
    ~LoginScreenPowerManagementPolicyHandler() {
}

void LoginScreenPowerManagementPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
}

DeprecatedIdleActionHandler::DeprecatedIdleActionHandler()
    : IntRangePolicyHandlerBase(
          key::kIdleAction,
          chromeos::PowerPolicyController::ACTION_SUSPEND,
          chromeos::PowerPolicyController::ACTION_DO_NOTHING,
          false) {}

DeprecatedIdleActionHandler::~DeprecatedIdleActionHandler() {}

void DeprecatedIdleActionHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                      PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(policy_name());
  if (value && EnsureInRange(value, NULL, NULL)) {
    if (!prefs->GetValue(prefs::kPowerAcIdleAction, NULL))
      prefs->SetValue(prefs::kPowerAcIdleAction, value->CreateDeepCopy());
    if (!prefs->GetValue(prefs::kPowerBatteryIdleAction, NULL))
      prefs->SetValue(prefs::kPowerBatteryIdleAction, value->CreateDeepCopy());
  }
}

PowerManagementIdleSettingsPolicyHandler::
    PowerManagementIdleSettingsPolicyHandler(const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kPowerManagementIdleSettings,
          chrome_schema.GetKnownProperty(key::kPowerManagementIdleSettings),
          SCHEMA_ALLOW_UNKNOWN) {
}

PowerManagementIdleSettingsPolicyHandler::
    ~PowerManagementIdleSettingsPolicyHandler() {
}

void PowerManagementIdleSettingsPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  scoped_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, NULL, &policy_value))
    return;
  const base::DictionaryValue* dict = NULL;
  if (!policy_value->GetAsDictionary(&dict)) {
    NOTREACHED();
    return;
  }
  scoped_ptr<base::Value> value;

  value = GetValue(dict, kScreenDimDelayAC);
  if (value)
    prefs->SetValue(prefs::kPowerAcScreenDimDelayMs, value.Pass());
  value = GetValue(dict, kScreenOffDelayAC);
  if (value)
    prefs->SetValue(prefs::kPowerAcScreenOffDelayMs, value.Pass());
  value = GetValue(dict, kIdleWarningDelayAC);
  if (value)
    prefs->SetValue(prefs::kPowerAcIdleWarningDelayMs, value.Pass());
  value = GetValue(dict, kIdleDelayAC);
  if (value)
    prefs->SetValue(prefs::kPowerAcIdleDelayMs, value.Pass());
  value = GetAction(dict, kIdleActionAC);
  if (value)
    prefs->SetValue(prefs::kPowerAcIdleAction, value.Pass());

  value = GetValue(dict, kScreenDimDelayBattery);
  if (value)
    prefs->SetValue(prefs::kPowerBatteryScreenDimDelayMs, value.Pass());
  value = GetValue(dict, kScreenOffDelayBattery);
  if (value)
    prefs->SetValue(prefs::kPowerBatteryScreenOffDelayMs, value.Pass());
  value = GetValue(dict, kIdleWarningDelayBattery);
  if (value)
    prefs->SetValue(prefs::kPowerBatteryIdleWarningDelayMs, value.Pass());
  value = GetValue(dict, kIdleDelayBattery);
  if (value)
    prefs->SetValue(prefs::kPowerBatteryIdleDelayMs, value.Pass());
  value = GetAction(dict, kIdleActionBattery);
  if (value)
    prefs->SetValue(prefs::kPowerBatteryIdleAction, value.Pass());
}

ScreenLockDelayPolicyHandler::ScreenLockDelayPolicyHandler(
    const Schema& chrome_schema)
    : SchemaValidatingPolicyHandler(
          key::kScreenLockDelays,
          chrome_schema.GetKnownProperty(key::kScreenLockDelays),
          SCHEMA_ALLOW_UNKNOWN) {
}

ScreenLockDelayPolicyHandler::~ScreenLockDelayPolicyHandler() {
}

void ScreenLockDelayPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  scoped_ptr<base::Value> policy_value;
  if (!CheckAndGetValue(policies, NULL, &policy_value))
    return;
  const base::DictionaryValue* dict = NULL;
  if (!policy_value->GetAsDictionary(&dict)) {
    NOTREACHED();
    return;
  }
  scoped_ptr<base::Value> value;

  value = GetValue(dict, kScreenLockDelayAC);
  if (value)
    prefs->SetValue(prefs::kPowerAcScreenLockDelayMs, value.Pass());
  value = GetValue(dict, kScreenLockDelayBattery);
  if (value)
    prefs->SetValue(prefs::kPowerBatteryScreenLockDelayMs, value.Pass());
}

}  // namespace policy
