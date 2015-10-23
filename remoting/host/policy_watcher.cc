// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this code is copied from:
//   src/chrome/browser/policy/asynchronous_policy_loader.{h,cc}

#include "remoting/host/policy_watcher.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_loader.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "policy/policy_constants.h"
#include "remoting/host/dns_blackhole_checker.h"
#include "remoting/host/third_party_auth_config.h"
#include "remoting/protocol/port_range.h"

#if !defined(NDEBUG)
#include "base/json/json_reader.h"
#endif

#if defined(OS_WIN)
#include "components/policy/core/common/policy_loader_win.h"
#elif defined(OS_MACOSX)
#include "components/policy/core/common/policy_loader_mac.h"
#include "components/policy/core/common/preferences_mac.h"
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
#include "components/policy/core/common/config_dir_policy_loader.h"
#endif

namespace remoting {

namespace key = ::policy::key;

namespace {

// Copies all policy values from one dictionary to another, using values from
// |default_values| if they are not set in |from|.
scoped_ptr<base::DictionaryValue> CopyValuesAndAddDefaults(
    const base::DictionaryValue& from,
    const base::DictionaryValue& default_values) {
  scoped_ptr<base::DictionaryValue> to(default_values.DeepCopy());
  for (base::DictionaryValue::Iterator i(default_values); !i.IsAtEnd();
       i.Advance()) {
    const base::Value* value = nullptr;

    // If the policy isn't in |from|, use the default.
    if (!from.Get(i.key(), &value)) {
      continue;
    }

    CHECK(value->IsType(i.value().GetType()));
    to->Set(i.key(), value->DeepCopy());
  }

#if !defined(NDEBUG)
  // Replace values with those specified in DebugOverridePolicies, if present.
  std::string policy_overrides;
  if (from.GetString(key::kRemoteAccessHostDebugOverridePolicies,
                     &policy_overrides)) {
    scoped_ptr<base::Value> value = base::JSONReader::Read(policy_overrides);
    const base::DictionaryValue* override_values;
    if (value && value->GetAsDictionary(&override_values)) {
      to->MergeDictionary(override_values);
    }
  }
#endif  // defined(NDEBUG)

  return to.Pass();
}

policy::PolicyNamespace GetPolicyNamespace() {
  return policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string());
}

scoped_ptr<policy::SchemaRegistry> CreateSchemaRegistry() {
  // TODO(lukasza): Schema below should ideally only cover Chromoting-specific
  // policies (expecting perf and maintanability improvement, but no functional
  // impact).
  policy::Schema schema = policy::Schema::Wrap(policy::GetChromeSchemaData());

  scoped_ptr<policy::SchemaRegistry> schema_registry(
      new policy::SchemaRegistry());
  schema_registry->RegisterComponent(GetPolicyNamespace(), schema);
  return schema_registry.Pass();
}

scoped_ptr<base::DictionaryValue> CopyChromotingPoliciesIntoDictionary(
    const policy::PolicyMap& current) {
  const char kPolicyNameSubstring[] = "RemoteAccessHost";
  scoped_ptr<base::DictionaryValue> policy_dict(new base::DictionaryValue());
  for (auto it = current.begin(); it != current.end(); ++it) {
    const std::string& key = it->first;
    const base::Value* value = it->second.value;

    // Copying only Chromoting-specific policies helps avoid false alarms
    // raised by NormalizePolicies below (such alarms shutdown the host).
    // TODO(lukasza): Removing this somewhat brittle filtering will be possible
    //                after having separate, Chromoting-specific schema.
    if (key.find(kPolicyNameSubstring) != std::string::npos) {
      policy_dict->Set(key, value->DeepCopy());
    }
  }

  return policy_dict.Pass();
}

// Takes a dictionary containing only 1) recognized policy names and 2)
// well-typed policy values and further verifies policy contents.
bool VerifyWellformedness(const base::DictionaryValue& changed_policies) {
  // Verify ThirdPartyAuthConfig policy.
  ThirdPartyAuthConfig not_used;
  switch (ThirdPartyAuthConfig::Parse(changed_policies, &not_used)) {
    case ThirdPartyAuthConfig::NoPolicy:
    case ThirdPartyAuthConfig::ParsingSuccess:
      break;  // Well-formed.
    case ThirdPartyAuthConfig::InvalidPolicy:
      return false;  // Malformed.
    default:
      NOTREACHED();
      return false;
  }

  // Verify UdpPortRange policy.
  std::string udp_port_range_string;
  PortRange udp_port_range;
  if (changed_policies.GetString(policy::key::kRemoteAccessHostUdpPortRange,
                                 &udp_port_range_string)) {
    if (!PortRange::Parse(udp_port_range_string, &udp_port_range)) {
      return false;
    }
  }

  // Report that all the policies were well-formed.
  return true;
}

}  // namespace

void PolicyWatcher::StartWatching(
    const PolicyUpdatedCallback& policy_updated_callback,
    const PolicyErrorCallback& policy_error_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!policy_updated_callback.is_null());
  DCHECK(!policy_error_callback.is_null());
  DCHECK(policy_updated_callback_.is_null());

  policy_updated_callback_ = policy_updated_callback;
  policy_error_callback_ = policy_error_callback;

  // Listen for future policy changes.
  policy_service_->AddObserver(policy::POLICY_DOMAIN_CHROME, this);

  // Process current policy state.
  if (policy_service_->IsInitializationComplete(policy::POLICY_DOMAIN_CHROME)) {
    OnPolicyServiceInitialized(policy::POLICY_DOMAIN_CHROME);
  }
}

void PolicyWatcher::SignalPolicyError() {
  old_policies_->Clear();
  policy_error_callback_.Run();
}

PolicyWatcher::PolicyWatcher(
    policy::PolicyService* policy_service,
    scoped_ptr<policy::PolicyService> owned_policy_service,
    scoped_ptr<policy::ConfigurationPolicyProvider> owned_policy_provider,
    scoped_ptr<policy::SchemaRegistry> owned_schema_registry)
    : old_policies_(new base::DictionaryValue()),
      default_values_(new base::DictionaryValue()),
      policy_service_(policy_service),
      owned_schema_registry_(owned_schema_registry.Pass()),
      owned_policy_provider_(owned_policy_provider.Pass()),
      owned_policy_service_(owned_policy_service.Pass()) {
  DCHECK(policy_service_);
  DCHECK(owned_schema_registry_);

  // Initialize the default values for each policy.
  default_values_->SetBoolean(key::kRemoteAccessHostFirewallTraversal, true);
  default_values_->SetBoolean(key::kRemoteAccessHostRequireCurtain, false);
  default_values_->SetBoolean(key::kRemoteAccessHostMatchUsername, false);
  default_values_->SetString(key::kRemoteAccessHostDomain, std::string());
  default_values_->SetString(key::kRemoteAccessHostTalkGadgetPrefix,
                             kDefaultHostTalkGadgetPrefix);
  default_values_->SetString(key::kRemoteAccessHostTokenUrl, std::string());
  default_values_->SetString(key::kRemoteAccessHostTokenValidationUrl,
                             std::string());
  default_values_->SetString(
      key::kRemoteAccessHostTokenValidationCertificateIssuer, std::string());
  default_values_->SetBoolean(key::kRemoteAccessHostAllowClientPairing, true);
  default_values_->SetBoolean(key::kRemoteAccessHostAllowGnubbyAuth, true);
  default_values_->SetBoolean(key::kRemoteAccessHostAllowRelayedConnection,
                              true);
  default_values_->SetString(key::kRemoteAccessHostUdpPortRange, "");
#if !defined(NDEBUG)
  default_values_->SetString(key::kRemoteAccessHostDebugOverridePolicies,
                             std::string());
#endif
}

PolicyWatcher::~PolicyWatcher() {
  // Stop observing |policy_service_| if StartWatching() has been called.
  if (!policy_updated_callback_.is_null()) {
    policy_service_->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  }

  if (owned_policy_provider_) {
    owned_policy_provider_->Shutdown();
  }
}

const policy::Schema* PolicyWatcher::GetPolicySchema() const {
  return owned_schema_registry_->schema_map()->GetSchema(GetPolicyNamespace());
}

bool PolicyWatcher::NormalizePolicies(base::DictionaryValue* policy_dict) {
  // Allowing unrecognized policy names allows presence of
  // 1) comments (i.e. JSON of the form: { "_comment": "blah", ... }),
  // 2) policies intended for future/newer versions of the host,
  // 3) policies not supported on all OS-s (i.e. RemoteAccessHostMatchUsername
  //    is not supported on Windows and therefore policy_templates.json omits
  //    schema for this policy on this particular platform).
  auto strategy = policy::SCHEMA_ALLOW_UNKNOWN_TOPLEVEL;

  std::string path;
  std::string error;
  bool changed = false;
  const policy::Schema* schema = GetPolicySchema();
  if (schema->Normalize(policy_dict, strategy, &path, &error, &changed)) {
    if (changed) {
      LOG(WARNING) << "Unknown (unrecognized or unsupported) policy: " << path
                   << ": " << error;
    }
    return true;
  } else {
    LOG(ERROR) << "Invalid policy contents: " << path << ": " << error;
    return false;
  }
}

namespace {
void CopyDictionaryValue(const base::DictionaryValue& from,
                         base::DictionaryValue& to,
                         std::string key) {
  const base::Value* value;
  if (from.Get(key, &value)) {
    to.Set(key, value->DeepCopy());
  }
}
}  // namespace

scoped_ptr<base::DictionaryValue>
PolicyWatcher::StoreNewAndReturnChangedPolicies(
    scoped_ptr<base::DictionaryValue> new_policies) {
  // Find the changed policies.
  scoped_ptr<base::DictionaryValue> changed_policies(
      new base::DictionaryValue());
  base::DictionaryValue::Iterator iter(*new_policies);
  while (!iter.IsAtEnd()) {
    base::Value* old_policy;
    if (!(old_policies_->Get(iter.key(), &old_policy) &&
          old_policy->Equals(&iter.value()))) {
      changed_policies->Set(iter.key(), iter.value().DeepCopy());
    }
    iter.Advance();
  }

  // If one of ThirdPartyAuthConfig policies changed, we need to include all.
  if (changed_policies->HasKey(key::kRemoteAccessHostTokenUrl) ||
      changed_policies->HasKey(key::kRemoteAccessHostTokenValidationUrl) ||
      changed_policies->HasKey(
          key::kRemoteAccessHostTokenValidationCertificateIssuer)) {
    CopyDictionaryValue(*new_policies, *changed_policies,
                        key::kRemoteAccessHostTokenUrl);
    CopyDictionaryValue(*new_policies, *changed_policies,
                        key::kRemoteAccessHostTokenValidationUrl);
    CopyDictionaryValue(*new_policies, *changed_policies,
                        key::kRemoteAccessHostTokenValidationCertificateIssuer);
  }

  // Save the new policies.
  old_policies_.swap(new_policies);

  return changed_policies.Pass();
}

void PolicyWatcher::OnPolicyUpdated(const policy::PolicyNamespace& ns,
                                    const policy::PolicyMap& previous,
                                    const policy::PolicyMap& current) {
  scoped_ptr<base::DictionaryValue> new_policies =
      CopyChromotingPoliciesIntoDictionary(current);

  // Check for mistyped values and get rid of unknown policies.
  if (!NormalizePolicies(new_policies.get())) {
    SignalPolicyError();
    return;
  }

  // Use default values for any missing policies.
  scoped_ptr<base::DictionaryValue> filled_policies =
      CopyValuesAndAddDefaults(*new_policies, *default_values_);

  // Limit reporting to only the policies that were changed.
  scoped_ptr<base::DictionaryValue> changed_policies =
      StoreNewAndReturnChangedPolicies(filled_policies.Pass());
  if (changed_policies->empty()) {
    return;
  }

  // Verify that we are calling the callback with valid policies.
  if (!VerifyWellformedness(*changed_policies)) {
    SignalPolicyError();
    return;
  }

  // Notify our client of the changed policies.
  policy_updated_callback_.Run(changed_policies.Pass());
}

void PolicyWatcher::OnPolicyServiceInitialized(policy::PolicyDomain domain) {
  policy::PolicyNamespace ns = GetPolicyNamespace();
  const policy::PolicyMap& current = policy_service_->GetPolicies(ns);
  OnPolicyUpdated(ns, current, current);
}

scoped_ptr<PolicyWatcher> PolicyWatcher::CreateFromPolicyLoader(
    scoped_ptr<policy::AsyncPolicyLoader> async_policy_loader) {
  scoped_ptr<policy::SchemaRegistry> schema_registry = CreateSchemaRegistry();
  scoped_ptr<policy::AsyncPolicyProvider> policy_provider(
      new policy::AsyncPolicyProvider(schema_registry.get(),
                                      async_policy_loader.Pass()));
  policy_provider->Init(schema_registry.get());

  policy::PolicyServiceImpl::Providers providers;
  providers.push_back(policy_provider.get());
  scoped_ptr<policy::PolicyService> policy_service(
      new policy::PolicyServiceImpl(providers));

  policy::PolicyService* borrowed_policy_service = policy_service.get();
  return make_scoped_ptr(
      new PolicyWatcher(borrowed_policy_service, policy_service.Pass(),
                        policy_provider.Pass(), schema_registry.Pass()));
}

scoped_ptr<PolicyWatcher> PolicyWatcher::Create(
    policy::PolicyService* policy_service,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_task_runner) {
#if defined(OS_CHROMEOS)
  // On Chrome OS the PolicyService is owned by the browser.
  DCHECK(policy_service);
  return make_scoped_ptr(new PolicyWatcher(policy_service, nullptr, nullptr,
                                           CreateSchemaRegistry()));
#else  // !defined(OS_CHROMEOS)
  DCHECK(!policy_service);

  // Create platform-specific PolicyLoader. Always read the Chrome policies
  // (even on Chromium) so that policy enforcement can't be bypassed by running
  // Chromium.
  scoped_ptr<policy::AsyncPolicyLoader> policy_loader;
#if defined(OS_WIN)
  policy_loader.reset(new policy::PolicyLoaderWin(
      file_task_runner, L"SOFTWARE\\Policies\\Google\\Chrome",
      nullptr));  // nullptr = don't use GPO / always read from the registry.
#elif defined(OS_MACOSX)
  CFStringRef bundle_id = CFSTR("com.google.Chrome");
  policy_loader.reset(new policy::PolicyLoaderMac(
      file_task_runner,
      policy::PolicyLoaderMac::GetManagedPolicyPath(bundle_id),
      new MacPreferences(), bundle_id));
#elif defined(OS_POSIX) && !defined(OS_ANDROID)
  policy_loader.reset(new policy::ConfigDirPolicyLoader(
      file_task_runner,
      base::FilePath(FILE_PATH_LITERAL("/etc/opt/chrome/policies")),
      policy::POLICY_SCOPE_MACHINE));
#else
#error OS that is not yet supported by PolicyWatcher code.
#endif

  return PolicyWatcher::CreateFromPolicyLoader(policy_loader.Pass());
#endif  // !(OS_CHROMEOS)
}

}  // namespace remoting
