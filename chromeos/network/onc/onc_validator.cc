// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_validator.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"

namespace chromeos {
namespace onc {

namespace {

// According to the IEEE 802.11 standard the SSID is a series of 0 to 32 octets.
const int kMaximumSSIDLengthInBytes = 32;

template <typename T, size_t N>
std::vector<T> toVector(T const (&array)[N]) {
  return std::vector<T>(array, array + N);
}

// Copied from policy/configuration_policy_handler.cc.
// TODO(pneubeck): move to a common place like base/.
std::string ValueTypeToString(base::Value::Type type) {
  const char* const strings[] = {"null",   "boolean", "integer",    "double",
                                 "string", "binary",  "dictionary", "list"};
  CHECK(static_cast<size_t>(type) < arraysize(strings));
  return strings[type];
}

}  // namespace

Validator::Validator(bool error_on_unknown_field,
                     bool error_on_wrong_recommended,
                     bool error_on_missing_field,
                     bool managed_onc)
    : error_on_unknown_field_(error_on_unknown_field),
      error_on_wrong_recommended_(error_on_wrong_recommended),
      error_on_missing_field_(error_on_missing_field),
      managed_onc_(managed_onc),
      onc_source_(::onc::ONC_SOURCE_NONE) {}

Validator::~Validator() {}

scoped_ptr<base::DictionaryValue> Validator::ValidateAndRepairObject(
    const OncValueSignature* object_signature,
    const base::DictionaryValue& onc_object,
    Result* result) {
  CHECK(object_signature);
  *result = VALID;
  error_or_warning_found_ = false;
  bool error = false;
  scoped_ptr<base::Value> result_value =
      MapValue(*object_signature, onc_object, &error);
  if (error) {
    *result = INVALID;
    result_value.reset();
  } else if (error_or_warning_found_) {
    *result = VALID_WITH_WARNINGS;
  }
  // The return value should be NULL if, and only if, |result| equals INVALID.
  DCHECK_EQ(result_value.get() == NULL, *result == INVALID);

  base::DictionaryValue* result_dict = NULL;
  if (result_value) {
    result_value.release()->GetAsDictionary(&result_dict);
    CHECK(result_dict);
  }

  return make_scoped_ptr(result_dict);
}

scoped_ptr<base::Value> Validator::MapValue(const OncValueSignature& signature,
                                            const base::Value& onc_value,
                                            bool* error) {
  if (onc_value.GetType() != signature.onc_type) {
    LOG(ERROR) << MessageHeader() << "Found value '" << onc_value
               << "' of type '" << ValueTypeToString(onc_value.GetType())
               << "', but type '" << ValueTypeToString(signature.onc_type)
               << "' is required.";
    error_or_warning_found_ = *error = true;
    return scoped_ptr<base::Value>();
  }

  scoped_ptr<base::Value> repaired =
      Mapper::MapValue(signature, onc_value, error);
  if (repaired)
    CHECK_EQ(repaired->GetType(), signature.onc_type);
  return repaired.Pass();
}

scoped_ptr<base::DictionaryValue> Validator::MapObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object,
    bool* error) {
  scoped_ptr<base::DictionaryValue> repaired(new base::DictionaryValue);

  bool valid = ValidateObjectDefault(signature, onc_object, repaired.get());
  if (valid) {
    if (&signature == &kToplevelConfigurationSignature) {
      valid = ValidateToplevelConfiguration(repaired.get());
    } else if (&signature == &kNetworkConfigurationSignature) {
      valid = ValidateNetworkConfiguration(repaired.get());
    } else if (&signature == &kEthernetSignature) {
      valid = ValidateEthernet(repaired.get());
    } else if (&signature == &kIPConfigSignature ||
               &signature == &kSavedIPConfigSignature ||
               &signature == &kStaticIPConfigSignature) {
      valid = ValidateIPConfig(repaired.get());
    } else if (&signature == &kWiFiSignature) {
      valid = ValidateWiFi(repaired.get());
    } else if (&signature == &kVPNSignature) {
      valid = ValidateVPN(repaired.get());
    } else if (&signature == &kIPsecSignature) {
      valid = ValidateIPsec(repaired.get());
    } else if (&signature == &kOpenVPNSignature) {
      valid = ValidateOpenVPN(repaired.get());
    } else if (&signature == &kThirdPartyVPNSignature) {
      valid = ValidateThirdPartyVPN(repaired.get());
    } else if (&signature == &kVerifyX509Signature) {
      valid = ValidateVerifyX509(repaired.get());
    } else if (&signature == &kCertificatePatternSignature) {
      valid = ValidateCertificatePattern(repaired.get());
    } else if (&signature == &kProxySettingsSignature) {
      valid = ValidateProxySettings(repaired.get());
    } else if (&signature == &kProxyLocationSignature) {
      valid = ValidateProxyLocation(repaired.get());
    } else if (&signature == &kEAPSignature) {
      valid = ValidateEAP(repaired.get());
    } else if (&signature == &kCertificateSignature) {
      valid = ValidateCertificate(repaired.get());
    }
  }

  if (valid) {
    return repaired.Pass();
  } else {
    DCHECK(error_or_warning_found_);
    error_or_warning_found_ = *error = true;
    return scoped_ptr<base::DictionaryValue>();
  }
}

scoped_ptr<base::Value> Validator::MapField(
    const std::string& field_name,
    const OncValueSignature& object_signature,
    const base::Value& onc_value,
    bool* found_unknown_field,
    bool* error) {
  path_.push_back(field_name);
  bool current_field_unknown = false;
  scoped_ptr<base::Value> result = Mapper::MapField(
      field_name, object_signature, onc_value, &current_field_unknown, error);

  DCHECK_EQ(field_name, path_.back());
  path_.pop_back();

  if (current_field_unknown) {
    error_or_warning_found_ = *found_unknown_field = true;
    std::string message = MessageHeader() + "Field name '" + field_name +
        "' is unknown.";
    if (error_on_unknown_field_)
      LOG(ERROR) << message;
    else
      LOG(WARNING) << message;
  }

  return result.Pass();
}

scoped_ptr<base::ListValue> Validator::MapArray(
    const OncValueSignature& array_signature,
    const base::ListValue& onc_array,
    bool* nested_error) {
  bool nested_error_in_current_array = false;
  scoped_ptr<base::ListValue> result = Mapper::MapArray(
      array_signature, onc_array, &nested_error_in_current_array);

  // Drop individual networks and certificates instead of rejecting all of
  // the configuration.
  if (nested_error_in_current_array &&
      &array_signature != &kNetworkConfigurationListSignature &&
      &array_signature != &kCertificateListSignature) {
    *nested_error = nested_error_in_current_array;
  }
  return result.Pass();
}

scoped_ptr<base::Value> Validator::MapEntry(int index,
                                            const OncValueSignature& signature,
                                            const base::Value& onc_value,
                                            bool* error) {
  std::string str = base::IntToString(index);
  path_.push_back(str);
  scoped_ptr<base::Value> result =
      Mapper::MapEntry(index, signature, onc_value, error);
  DCHECK_EQ(str, path_.back());
  path_.pop_back();
  return result.Pass();
}

bool Validator::ValidateObjectDefault(const OncValueSignature& signature,
                                      const base::DictionaryValue& onc_object,
                                      base::DictionaryValue* result) {
  bool found_unknown_field = false;
  bool nested_error_occured = false;
  MapFields(signature, onc_object, &found_unknown_field, &nested_error_occured,
            result);

  if (found_unknown_field && error_on_unknown_field_) {
    DVLOG(1) << "Unknown field names are errors: Aborting.";
    return false;
  }

  if (nested_error_occured)
    return false;

  return ValidateRecommendedField(signature, result);
}

bool Validator::ValidateRecommendedField(
    const OncValueSignature& object_signature,
    base::DictionaryValue* result) {
  CHECK(result);

  scoped_ptr<base::Value> recommended_value;
  // This remove passes ownership to |recommended_value|.
  if (!result->RemoveWithoutPathExpansion(::onc::kRecommended,
                                          &recommended_value)) {
    return true;
  }

  base::ListValue* recommended_list = nullptr;
  recommended_value->GetAsList(&recommended_list);
  DCHECK(recommended_list);  // The types of field values are already verified.

  if (!managed_onc_) {
    error_or_warning_found_ = true;
    LOG(WARNING) << MessageHeader() << "Found the field '"
                 << ::onc::kRecommended
                 << "' in an unmanaged ONC. Removing it.";
    return true;
  }

  scoped_ptr<base::ListValue> repaired_recommended(new base::ListValue);
  for (const base::Value* entry : *recommended_list) {
    std::string field_name;
    if (!entry->GetAsString(&field_name)) {
      NOTREACHED();  // The types of field values are already verified.
      continue;
    }

    const OncFieldSignature* field_signature =
        GetFieldSignature(object_signature, field_name);

    bool found_error = false;
    std::string error_cause;
    if (!field_signature) {
      found_error = true;
      error_cause = "unknown";
    } else if (field_signature->value_signature->onc_type ==
               base::Value::TYPE_DICTIONARY) {
      found_error = true;
      error_cause = "dictionary-typed";
    }

    if (found_error) {
      error_or_warning_found_ = true;
      path_.push_back(::onc::kRecommended);
      std::string message = MessageHeader() + "The " + error_cause +
          " field '" + field_name + "' cannot be recommended.";
      path_.pop_back();
      if (error_on_wrong_recommended_) {
        LOG(ERROR) << message;
        return false;
      } else {
        LOG(WARNING) << message;
        continue;
      }
    }

    repaired_recommended->AppendString(field_name);
  }

  result->Set(::onc::kRecommended, repaired_recommended.release());
  return true;
}

bool Validator::ValidateClientCertFields(bool allow_cert_type_none,
                                         base::DictionaryValue* result) {
  using namespace ::onc::client_cert;
  const char* const kValidCertTypes[] = {kRef, kPattern};
  std::vector<const char*> valid_cert_types(toVector(kValidCertTypes));
  if (allow_cert_type_none)
    valid_cert_types.push_back(kClientCertTypeNone);
  if (FieldExistsAndHasNoValidValue(*result, kClientCertType, valid_cert_types))
    return false;

  std::string cert_type;
  result->GetStringWithoutPathExpansion(kClientCertType, &cert_type);

  if (IsCertPatternInDevicePolicy(cert_type))
    return false;

  bool all_required_exist = true;

  if (cert_type == kPattern)
    all_required_exist &= RequireField(*result, kClientCertPattern);
  else if (cert_type == kRef)
    all_required_exist &= RequireField(*result, kClientCertRef);

  return !error_on_missing_field_ || all_required_exist;
}

namespace {

std::string JoinStringRange(const std::vector<const char*>& strings,
                            const std::string& separator) {
  std::vector<std::string> string_vector;
  std::copy(strings.begin(), strings.end(), std::back_inserter(string_vector));
  return JoinString(string_vector, separator);
}

}  // namespace

bool Validator::FieldExistsAndHasNoValidValue(
    const base::DictionaryValue& object,
    const std::string& field_name,
    const std::vector<const char*>& valid_values) {
  std::string actual_value;
  if (!object.GetStringWithoutPathExpansion(field_name, &actual_value))
    return false;

  for (std::vector<const char*>::const_iterator it = valid_values.begin();
       it != valid_values.end();
       ++it) {
    if (actual_value == *it)
      return false;
  }
  error_or_warning_found_ = true;
  std::string valid_values_str =
      "[" + JoinStringRange(valid_values, ", ") + "]";
  path_.push_back(field_name);
  LOG(ERROR) << MessageHeader() << "Found value '" << actual_value <<
      "', but expected one of the values " << valid_values_str;
  path_.pop_back();
  return true;
}

bool Validator::FieldExistsAndIsNotInRange(const base::DictionaryValue& object,
                                           const std::string& field_name,
                                           int lower_bound,
                                           int upper_bound) {
  int actual_value;
  if (!object.GetIntegerWithoutPathExpansion(field_name, &actual_value) ||
      (lower_bound <= actual_value && actual_value <= upper_bound)) {
    return false;
  }
  error_or_warning_found_ = true;
  path_.push_back(field_name);
  LOG(ERROR) << MessageHeader() << "Found value '" << actual_value
             << "', but expected a value in the range [" << lower_bound
             << ", " << upper_bound << "] (boundaries inclusive)";
  path_.pop_back();
  return true;
}

bool Validator::FieldExistsAndIsEmpty(const base::DictionaryValue& object,
                                      const std::string& field_name) {
  const base::Value* value = NULL;
  if (!object.GetWithoutPathExpansion(field_name, &value))
    return false;

  std::string str;
  const base::ListValue* list = NULL;
  if (value->GetAsString(&str)) {
    if (!str.empty())
      return false;
  } else if (value->GetAsList(&list)) {
    if (!list->empty())
      return false;
  } else {
    NOTREACHED();
    return false;
  }

  error_or_warning_found_ = true;
  path_.push_back(field_name);
  LOG(ERROR) << MessageHeader() << "Found an empty string, but expected a "
             << "non-empty string.";
  path_.pop_back();
  return true;
}

bool Validator::ValidateSSIDAndHexSSID(base::DictionaryValue* object) {
  // Check SSID validity.
  std::string ssid_string;
  if (object->GetStringWithoutPathExpansion(::onc::wifi::kSSID, &ssid_string) &&
      (ssid_string.size() <= 0 ||
       ssid_string.size() > kMaximumSSIDLengthInBytes)) {
    error_or_warning_found_ = true;
    const std::string msg =
        MessageHeader() + ::onc::wifi::kSSID + " has an invalid length.";
    // If the HexSSID field is present, ignore errors in SSID because these
    // might be caused by the usage of a non-UTF-8 encoding when the SSID
    // field was automatically added (see FillInHexSSIDField).
    if (object->HasKey(::onc::wifi::kHexSSID)) {
      LOG(WARNING) << msg;
    } else {
      LOG(ERROR) << msg;
      return false;
    }
  }

  // Check HexSSID validity.
  std::string hex_ssid_string;
  if (object->GetStringWithoutPathExpansion(::onc::wifi::kHexSSID,
                                            &hex_ssid_string)) {
    std::vector<uint8> decoded_ssid;
    if (!base::HexStringToBytes(hex_ssid_string, &decoded_ssid)) {
      LOG(ERROR) << MessageHeader() << "Field " << ::onc::wifi::kHexSSID
                 << " is not a valid hex representation: \"" << hex_ssid_string
                 << "\"";
      error_or_warning_found_ = true;
      return false;
    }
    if (decoded_ssid.size() <= 0 ||
        decoded_ssid.size() > kMaximumSSIDLengthInBytes) {
      LOG(ERROR) << MessageHeader() << ::onc::wifi::kHexSSID
                 << " has an invalid length.";
      error_or_warning_found_ = true;
      return false;
    }

    // If both SSID and HexSSID are set, check whether they are consistent, i.e.
    // HexSSID contains the UTF-8 encoding of SSID. If not, remove the SSID
    // field.
    if (ssid_string.length() > 0) {
      std::string decoded_ssid_string(
          reinterpret_cast<const char*>(&decoded_ssid[0]), decoded_ssid.size());
      if (ssid_string != decoded_ssid_string) {
        LOG(WARNING) << MessageHeader() << "Fields " << ::onc::wifi::kSSID
                     << " and " << ::onc::wifi::kHexSSID
                     << " contain inconsistent values. Removing "
                     << ::onc::wifi::kSSID << ".";
        error_or_warning_found_ = true;
        object->RemoveWithoutPathExpansion(::onc::wifi::kSSID, nullptr);
      }
    }
  }
  return true;
}

bool Validator::RequireField(const base::DictionaryValue& dict,
                             const std::string& field_name) {
  if (dict.HasKey(field_name))
    return true;
  std::string message = MessageHeader() + "The required field '" + field_name +
      "' is missing.";
  if (error_on_missing_field_) {
    error_or_warning_found_ = true;
    LOG(ERROR) << message;
  } else {
    VLOG(1) << message;
  }
  return false;
}

bool Validator::CheckGuidIsUniqueAndAddToSet(const base::DictionaryValue& dict,
                                             const std::string& key_guid,
                                             std::set<std::string> *guids) {
  std::string guid;
  if (dict.GetStringWithoutPathExpansion(key_guid, &guid)) {
    if (guids->count(guid) != 0) {
      error_or_warning_found_ = true;
      LOG(ERROR) << MessageHeader() << "Found a duplicate GUID " << guid << ".";
      return false;
    }
    guids->insert(guid);
  }
  return true;
}

bool Validator::IsCertPatternInDevicePolicy(const std::string& cert_type) {
  if (cert_type == ::onc::client_cert::kPattern &&
      onc_source_ == ::onc::ONC_SOURCE_DEVICE_POLICY) {
    error_or_warning_found_ = true;
    LOG(ERROR) << MessageHeader() << "Client certificate patterns are "
               << "prohibited in ONC device policies.";
    return true;
  }
  return false;
}

bool Validator::IsGlobalNetworkConfigInUserImport(
    const base::DictionaryValue& onc_object) {
  if (onc_source_ == ::onc::ONC_SOURCE_USER_IMPORT &&
      onc_object.HasKey(::onc::toplevel_config::kGlobalNetworkConfiguration)) {
    error_or_warning_found_ = true;
    LOG(ERROR) << MessageHeader() << "GlobalNetworkConfiguration is prohibited "
               << "in ONC user imports";
    return true;
  }
  return false;
}

bool Validator::ValidateToplevelConfiguration(base::DictionaryValue* result) {
  using namespace ::onc::toplevel_config;

  const char* const kValidTypes[] = {kUnencryptedConfiguration,
                                     kEncryptedConfiguration};
  const std::vector<const char*> valid_types(toVector(kValidTypes));
  if (FieldExistsAndHasNoValidValue(*result, kType, valid_types))
    return false;

  if (IsGlobalNetworkConfigInUserImport(*result))
    return false;

  return true;
}

bool Validator::ValidateNetworkConfiguration(base::DictionaryValue* result) {
  using namespace ::onc::network_config;

  const char* const kValidTypes[] = {::onc::network_type::kEthernet,
                                     ::onc::network_type::kVPN,
                                     ::onc::network_type::kWiFi,
                                     ::onc::network_type::kCellular,
                                     ::onc::network_type::kWimax};
  const std::vector<const char*> valid_types(toVector(kValidTypes));
  const char* const kValidIPConfigTypes[] = {kIPConfigTypeDHCP,
                                             kIPConfigTypeStatic};
  const std::vector<const char*> valid_ipconfig_types(
      toVector(kValidIPConfigTypes));
  if (FieldExistsAndHasNoValidValue(*result, kType, valid_types) ||
      FieldExistsAndHasNoValidValue(*result, kIPAddressConfigType,
                                    valid_ipconfig_types) ||
      FieldExistsAndHasNoValidValue(*result, kNameServersConfigType,
                                    valid_ipconfig_types) ||
      FieldExistsAndIsEmpty(*result, kGUID)) {
    return false;
  }

  if (!CheckGuidIsUniqueAndAddToSet(*result, kGUID, &network_guids_))
    return false;

  bool all_required_exist = RequireField(*result, kGUID);

  bool remove = false;
  result->GetBooleanWithoutPathExpansion(::onc::kRemove, &remove);
  if (!remove) {
    all_required_exist &=
        RequireField(*result, kName) && RequireField(*result, kType);

    std::string ip_address_config_type, name_servers_config_type;
    result->GetStringWithoutPathExpansion(kIPAddressConfigType,
                                          &ip_address_config_type);
    result->GetStringWithoutPathExpansion(kNameServersConfigType,
                                          &name_servers_config_type);
    if (ip_address_config_type == kIPConfigTypeStatic ||
        name_servers_config_type == kIPConfigTypeStatic) {
      // TODO(pneubeck): Add ValidateStaticIPConfig and confirm that the
      // correct properties are provided based on the config type.
      all_required_exist &= RequireField(*result, kStaticIPConfig);
    }

    std::string type;
    result->GetStringWithoutPathExpansion(kType, &type);

    // Prohibit anything but WiFi and Ethernet for device-level policy (which
    // corresponds to shared networks). See also http://crosbug.com/28741.
    if (onc_source_ == ::onc::ONC_SOURCE_DEVICE_POLICY && !type.empty() &&
        type != ::onc::network_type::kWiFi &&
        type != ::onc::network_type::kEthernet) {
      error_or_warning_found_ = true;
      LOG(ERROR) << MessageHeader() << "Networks of type '"
                 << type << "' are prohibited in ONC device policies.";
      return false;
    }

    if (type == ::onc::network_type::kWiFi) {
      all_required_exist &= RequireField(*result, ::onc::network_config::kWiFi);
    } else if (type == ::onc::network_type::kEthernet) {
      all_required_exist &=
          RequireField(*result, ::onc::network_config::kEthernet);
    } else if (type == ::onc::network_type::kCellular) {
      all_required_exist &=
          RequireField(*result, ::onc::network_config::kCellular);
    } else if (type == ::onc::network_type::kWimax) {
      all_required_exist &=
          RequireField(*result, ::onc::network_config::kWimax);
    } else if (type == ::onc::network_type::kVPN) {
      all_required_exist &= RequireField(*result, ::onc::network_config::kVPN);
    }
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateEthernet(base::DictionaryValue* result) {
  using namespace ::onc::ethernet;

  const char* const kValidAuthentications[] = {kAuthenticationNone, k8021X};
  const std::vector<const char*> valid_authentications(
      toVector(kValidAuthentications));
  if (FieldExistsAndHasNoValidValue(
          *result, kAuthentication, valid_authentications)) {
    return false;
  }

  bool all_required_exist = true;
  std::string auth;
  result->GetStringWithoutPathExpansion(kAuthentication, &auth);
  if (auth == k8021X)
    all_required_exist &= RequireField(*result, kEAP);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateIPConfig(base::DictionaryValue* result) {
  using namespace ::onc::ipconfig;

  const char* const kValidTypes[] = {kIPv4, kIPv6};
  const std::vector<const char*> valid_types(toVector(kValidTypes));
  if (FieldExistsAndHasNoValidValue(
          *result, ::onc::ipconfig::kType, valid_types))
    return false;

  std::string type;
  result->GetStringWithoutPathExpansion(::onc::ipconfig::kType, &type);
  int lower_bound = 1;
  // In case of missing type, choose higher upper_bound.
  int upper_bound = (type == kIPv4) ? 32 : 128;
  if (FieldExistsAndIsNotInRange(
          *result, kRoutingPrefix, lower_bound, upper_bound)) {
    return false;
  }

  bool all_required_exist = RequireField(*result, kIPAddress) &&
                            RequireField(*result, ::onc::ipconfig::kType);
  if (result->HasKey(kIPAddress))
    all_required_exist &= RequireField(*result, kRoutingPrefix);


  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateWiFi(base::DictionaryValue* result) {
  using namespace ::onc::wifi;

  const char* const kValidSecurities[] = {kSecurityNone, kWEP_PSK, kWEP_8021X,
                                          kWPA_PSK, kWPA_EAP};
  const std::vector<const char*> valid_securities(toVector(kValidSecurities));
  if (FieldExistsAndHasNoValidValue(*result, kSecurity, valid_securities))
    return false;

  if (!ValidateSSIDAndHexSSID(result))
    return false;

  bool all_required_exist = RequireField(*result, kSecurity);

  // One of {kSSID, kHexSSID} must be present.
  if (!result->HasKey(kSSID))
    all_required_exist &= RequireField(*result, kHexSSID);
  if (!result->HasKey(kHexSSID))
    all_required_exist &= RequireField(*result, kSSID);

  std::string security;
  result->GetStringWithoutPathExpansion(kSecurity, &security);
  if (security == kWEP_8021X || security == kWPA_EAP)
    all_required_exist &= RequireField(*result, kEAP);
  else if (security == kWEP_PSK || security == kWPA_PSK)
    all_required_exist &= RequireField(*result, kPassphrase);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateVPN(base::DictionaryValue* result) {
  using namespace ::onc::vpn;

  const char* const kValidTypes[] = {
      kIPsec, kTypeL2TP_IPsec, kOpenVPN, kThirdPartyVpn};
  const std::vector<const char*> valid_types(toVector(kValidTypes));
  if (FieldExistsAndHasNoValidValue(*result, ::onc::vpn::kType, valid_types))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::vpn::kType);
  std::string type;
  result->GetStringWithoutPathExpansion(::onc::vpn::kType, &type);
  if (type == kOpenVPN) {
    all_required_exist &= RequireField(*result, kOpenVPN);
  } else if (type == kIPsec) {
    all_required_exist &= RequireField(*result, kIPsec);
  } else if (type == kTypeL2TP_IPsec) {
    all_required_exist &=
        RequireField(*result, kIPsec) && RequireField(*result, kL2TP);
  } else if (type == kThirdPartyVpn) {
    all_required_exist &= RequireField(*result, kThirdPartyVpn);
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateIPsec(base::DictionaryValue* result) {
  using namespace ::onc::ipsec;

  const char* const kValidAuthentications[] = {kPSK, kCert};
  const std::vector<const char*> valid_authentications(
      toVector(kValidAuthentications));
  if (FieldExistsAndHasNoValidValue(
          *result, kAuthenticationType, valid_authentications) ||
      FieldExistsAndIsEmpty(*result, kServerCARefs)) {
    return false;
  }

  if (result->HasKey(kServerCARefs) && result->HasKey(kServerCARef)) {
    error_or_warning_found_ = true;
    LOG(ERROR) << MessageHeader() << "At most one of " << kServerCARefs
               << " and " << kServerCARef << " can be set.";
    return false;
  }

  if (!ValidateClientCertFields(false,  // don't allow ClientCertType None
                                result)) {
    return false;
  }

  bool all_required_exist = RequireField(*result, kAuthenticationType) &&
                            RequireField(*result, kIKEVersion);
  std::string auth;
  result->GetStringWithoutPathExpansion(kAuthenticationType, &auth);
  bool has_server_ca_cert =
      result->HasKey(kServerCARefs) || result->HasKey(kServerCARef);
  if (auth == kCert) {
    all_required_exist &=
        RequireField(*result, ::onc::client_cert::kClientCertType);
    if (!has_server_ca_cert) {
      all_required_exist = false;
      error_or_warning_found_ = true;
      std::string message = MessageHeader() + "The required field '" +
                            kServerCARefs + "' is missing.";
      if (error_on_missing_field_)
        LOG(ERROR) << message;
      else
        LOG(WARNING) << message;
    }
  } else if (has_server_ca_cert) {
    error_or_warning_found_ = true;
    LOG(ERROR) << MessageHeader() << kServerCARefs << " (or " << kServerCARef
               << ") can only be set if " << kAuthenticationType
               << " is set to " << kCert << ".";
    return false;
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateOpenVPN(base::DictionaryValue* result) {
  using namespace ::onc::openvpn;

  const char* const kValidAuthRetryValues[] = {::onc::openvpn::kNone, kInteract,
                                               kNoInteract};
  const std::vector<const char*> valid_auth_retry_values(
      toVector(kValidAuthRetryValues));
  const char* const kValidCertTlsValues[] = {::onc::openvpn::kNone,
                                             ::onc::openvpn::kServer};
  const std::vector<const char*> valid_cert_tls_values(
      toVector(kValidCertTlsValues));
  const char* const kValidUserAuthTypes[] = {
      ::onc::openvpn_user_auth_type::kNone,
      ::onc::openvpn_user_auth_type::kOTP,
      ::onc::openvpn_user_auth_type::kPassword,
      ::onc::openvpn_user_auth_type::kPasswordAndOTP};
  const std::vector<const char*> valid_user_auth_types(
      toVector(kValidUserAuthTypes));

  if (FieldExistsAndHasNoValidValue(
          *result, kAuthRetry, valid_auth_retry_values) ||
      FieldExistsAndHasNoValidValue(
          *result, kRemoteCertTLS, valid_cert_tls_values) ||
      FieldExistsAndHasNoValidValue(
          *result, kUserAuthenticationType, valid_user_auth_types) ||
      FieldExistsAndIsEmpty(*result, kServerCARefs)) {
    return false;
  }

  if (result->HasKey(kServerCARefs) && result->HasKey(kServerCARef)) {
    error_or_warning_found_ = true;
    LOG(ERROR) << MessageHeader() << "At most one of " << kServerCARefs
               << " and " << kServerCARef << " can be set.";
    return false;
  }

  if (!ValidateClientCertFields(true /* allow ClientCertType None */, result))
    return false;

  bool all_required_exist =
      RequireField(*result, ::onc::client_cert::kClientCertType);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateThirdPartyVPN(base::DictionaryValue* result) {
  const bool all_required_exist =
      RequireField(*result, ::onc::third_party_vpn::kExtensionID);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateVerifyX509(base::DictionaryValue* result) {
  using namespace ::onc::verify_x509;

  const char* const kValidTypes[] = {types::kName, types::kNamePrefix,
                                     types::kSubject};
  const std::vector<const char*> valid_types(toVector(kValidTypes));

  if (FieldExistsAndHasNoValidValue(*result, kType, valid_types))
    return false;

  bool all_required_exist = RequireField(*result, kName);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateCertificatePattern(base::DictionaryValue* result) {
  using namespace ::onc::client_cert;

  bool all_required_exist = true;
  if (!result->HasKey(kSubject) && !result->HasKey(kIssuer) &&
      !result->HasKey(kIssuerCARef)) {
    error_or_warning_found_ = true;
    all_required_exist = false;
    std::string message = MessageHeader() + "None of the fields '" + kSubject +
        "', '" + kIssuer + "', and '" + kIssuerCARef +
        "' is present, but at least one is required.";
    if (error_on_missing_field_)
      LOG(ERROR) << message;
    else
      LOG(WARNING) << message;
  }

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateProxySettings(base::DictionaryValue* result) {
  using namespace ::onc::proxy;

  const char* const kValidTypes[] = {kDirect, kManual, kPAC, kWPAD};
  const std::vector<const char*> valid_types(toVector(kValidTypes));
  if (FieldExistsAndHasNoValidValue(*result, ::onc::proxy::kType, valid_types))
    return false;

  bool all_required_exist = RequireField(*result, ::onc::proxy::kType);
  std::string type;
  result->GetStringWithoutPathExpansion(::onc::proxy::kType, &type);
  if (type == kManual)
    all_required_exist &= RequireField(*result, kManual);
  else if (type == kPAC)
    all_required_exist &= RequireField(*result, kPAC);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateProxyLocation(base::DictionaryValue* result) {
  using namespace ::onc::proxy;

  bool all_required_exist =
      RequireField(*result, kHost) && RequireField(*result, kPort);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateEAP(base::DictionaryValue* result) {
  using namespace ::onc::eap;

  const char* const kValidInnerValues[] = {
      kAutomatic, kGTC, kMD5, kMSCHAPv2, kPAP};
  const std::vector<const char*> valid_inner_values(
      toVector(kValidInnerValues));
  const char* const kValidOuterValues[] = {
      kPEAP, kEAP_TLS, kEAP_TTLS, kLEAP, kEAP_SIM, kEAP_FAST, kEAP_AKA};
  const std::vector<const char*> valid_outer_values(
      toVector(kValidOuterValues));

  if (FieldExistsAndHasNoValidValue(*result, kInner, valid_inner_values) ||
      FieldExistsAndHasNoValidValue(*result, kOuter, valid_outer_values) ||
      FieldExistsAndIsEmpty(*result, kServerCARefs)) {
    return false;
  }

  if (result->HasKey(kServerCARefs) && result->HasKey(kServerCARef)) {
    error_or_warning_found_ = true;
    LOG(ERROR) << MessageHeader() << "At most one of " << kServerCARefs
               << " and " << kServerCARef << " can be set.";
    return false;
  }

  if (!ValidateClientCertFields(false,  // don't allow ClientCertType None
                                result)) {
    return false;
  }

  bool all_required_exist = RequireField(*result, kOuter);

  return !error_on_missing_field_ || all_required_exist;
}

bool Validator::ValidateCertificate(base::DictionaryValue* result) {
  using namespace ::onc::certificate;

  const char* const kValidTypes[] = {kClient, kServer, kAuthority};
  const std::vector<const char*> valid_types(toVector(kValidTypes));
  if (FieldExistsAndHasNoValidValue(*result, kType, valid_types) ||
      FieldExistsAndIsEmpty(*result, kGUID)) {
    return false;
  }

  std::string type;
  result->GetStringWithoutPathExpansion(kType, &type);
  if (onc_source_ == ::onc::ONC_SOURCE_DEVICE_POLICY &&
      (type == kServer || type == kAuthority)) {
    error_or_warning_found_ = true;
    LOG(ERROR) << MessageHeader() << "Server and authority certificates are "
               << "prohibited in ONC device policies.";
    return false;
  }

  if (!CheckGuidIsUniqueAndAddToSet(*result, kGUID, &certificate_guids_))
    return false;

  bool all_required_exist = RequireField(*result, kGUID);

  bool remove = false;
  result->GetBooleanWithoutPathExpansion(::onc::kRemove, &remove);
  if (!remove) {
    all_required_exist &= RequireField(*result, kType);

    if (type == kClient)
      all_required_exist &= RequireField(*result, kPKCS12);
    else if (type == kServer || type == kAuthority)
      all_required_exist &= RequireField(*result, kX509);
  }

  return !error_on_missing_field_ || all_required_exist;
}

std::string Validator::MessageHeader() {
  std::string path = path_.empty() ? "toplevel" : JoinString(path_, ".");
  std::string message = "At " + path + ": ";
  return message;
}

}  // namespace onc
}  // namespace chromeos
