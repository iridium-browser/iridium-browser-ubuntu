// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/prefs/pref_service_mock_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::_;

namespace policy {

namespace {

void assertProxyMode(const ProxyConfigDictionary& dict,
                     ProxyPrefs::ProxyMode expected_mode) {
  ProxyPrefs::ProxyMode actual_mode;
  ASSERT_TRUE(dict.GetMode(&actual_mode));
  EXPECT_EQ(expected_mode, actual_mode);
}

void assertProxyServer(const ProxyConfigDictionary& dict,
                       const std::string& expected) {
  std::string actual;
  if (!expected.empty()) {
    ASSERT_TRUE(dict.GetProxyServer(&actual));
    EXPECT_EQ(expected, actual);
  } else {
    EXPECT_FALSE(dict.GetProxyServer(&actual));
  }
}

void assertPacUrl(const ProxyConfigDictionary& dict,
                  const std::string& expected) {
  std::string actual;
  if (!expected.empty()) {
    ASSERT_TRUE(dict.GetPacUrl(&actual));
    EXPECT_EQ(expected, actual);
  } else {
    EXPECT_FALSE(dict.GetPacUrl(&actual));
  }
}

void assertBypassList(const ProxyConfigDictionary& dict,
                      const std::string& expected) {
  std::string actual;
  if (!expected.empty()) {
    ASSERT_TRUE(dict.GetBypassList(&actual));
    EXPECT_EQ(expected, actual);
  } else {
    EXPECT_FALSE(dict.GetBypassList(&actual));
  }
}

void assertProxyModeWithoutParams(const ProxyConfigDictionary& dict,
                                  ProxyPrefs::ProxyMode proxy_mode) {
  assertProxyMode(dict, proxy_mode);
  assertProxyServer(dict, std::string());
  assertPacUrl(dict, std::string());
  assertBypassList(dict, std::string());
}

}  // namespace

class ProxyPolicyTest : public testing::Test {
 protected:
  ProxyPolicyTest() : command_line_(base::CommandLine::NO_PROGRAM) {}

  void SetUp() override {
    EXPECT_CALL(provider_, IsInitializationComplete(_))
        .WillRepeatedly(Return(true));

    PolicyServiceImpl::Providers providers;
    providers.push_back(&provider_);
    policy_service_.reset(new PolicyServiceImpl(providers));
    provider_.Init();
  }

  void TearDown() override { provider_.Shutdown(); }

  scoped_ptr<PrefService> CreatePrefService(bool with_managed_policies) {
    PrefServiceMockFactory factory;
    factory.SetCommandLine(&command_line_);
    if (with_managed_policies)
      factory.SetManagedPolicies(policy_service_.get());
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    scoped_ptr<PrefServiceSyncable> prefs =
        factory.CreateSyncable(registry.get());
    chrome::RegisterUserProfilePrefs(registry.get());
    return prefs.Pass();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::CommandLine command_line_;
  MockConfigurationPolicyProvider provider_;
  scoped_ptr<PolicyServiceImpl> policy_service_;
};

TEST_F(ProxyPolicyTest, OverridesCommandLineOptions) {
  command_line_.AppendSwitchASCII(switches::kProxyBypassList, "123");
  command_line_.AppendSwitchASCII(switches::kProxyServer, "789");
  base::Value* mode_name =
      new base::StringValue(ProxyPrefs::kFixedServersProxyModeName);
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             mode_name, NULL);
  policy.Set(key::kProxyBypassList,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("abc"),
             NULL);
  policy.Set(key::kProxyServer,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("ghi"),
             NULL);
  provider_.UpdateChromePolicy(policy);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  scoped_ptr<PrefService> prefs(CreatePrefService(false));
  ProxyConfigDictionary dict(prefs->GetDictionary(prefs::kProxy));
  assertProxyMode(dict, ProxyPrefs::MODE_FIXED_SERVERS);
  assertProxyServer(dict, "789");
  assertPacUrl(dict, std::string());
  assertBypassList(dict, "123");

  // Try a second time time with the managed PrefStore in place, the
  // manual proxy policy should have removed all traces of the command
  // line and replaced them with the policy versions.
  prefs = CreatePrefService(true);
  ProxyConfigDictionary dict2(prefs->GetDictionary(prefs::kProxy));
  assertProxyMode(dict2, ProxyPrefs::MODE_FIXED_SERVERS);
  assertProxyServer(dict2, "ghi");
  assertPacUrl(dict2, std::string());
  assertBypassList(dict2, "abc");
}

TEST_F(ProxyPolicyTest, OverridesUnrelatedCommandLineOptions) {
  command_line_.AppendSwitchASCII(switches::kProxyBypassList, "123");
  command_line_.AppendSwitchASCII(switches::kProxyServer, "789");
  base::Value* mode_name =
      new base::StringValue(ProxyPrefs::kAutoDetectProxyModeName);
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             mode_name, NULL);
  provider_.UpdateChromePolicy(policy);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  scoped_ptr<PrefService> prefs = CreatePrefService(false);
  ProxyConfigDictionary dict(prefs->GetDictionary(prefs::kProxy));
  assertProxyMode(dict, ProxyPrefs::MODE_FIXED_SERVERS);
  assertProxyServer(dict, "789");
  assertPacUrl(dict, std::string());
  assertBypassList(dict, "123");

  // Try a second time time with the managed PrefStore in place, the
  // no proxy policy should have removed all traces of the command
  // line proxy settings, even though they were not the specific one
  // set in policy.
  prefs = CreatePrefService(true);
  ProxyConfigDictionary dict2(prefs->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict2, ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyTest, OverridesCommandLineNoProxy) {
  command_line_.AppendSwitch(switches::kNoProxyServer);
  base::Value* mode_name =
      new base::StringValue(ProxyPrefs::kAutoDetectProxyModeName);
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             mode_name, NULL);
  provider_.UpdateChromePolicy(policy);

  // First verify that command-line options are set correctly when
  // there is no policy in effect.
  scoped_ptr<PrefService> prefs = CreatePrefService(false);
  ProxyConfigDictionary dict(prefs->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict, ProxyPrefs::MODE_DIRECT);

  // Try a second time time with the managed PrefStore in place, the
  // auto-detect should be overridden. The default pref store must be
  // in place with the appropriate default value for this to work.
  prefs = CreatePrefService(true);
  ProxyConfigDictionary dict2(prefs->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict2, ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyTest, OverridesCommandLineAutoDetect) {
  command_line_.AppendSwitch(switches::kProxyAutoDetect);
  base::Value* mode_name =
      new base::StringValue(ProxyPrefs::kDirectProxyModeName);
  PolicyMap policy;
  policy.Set(key::kProxyMode, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
             mode_name, NULL);
  provider_.UpdateChromePolicy(policy);

  // First verify that the auto-detect is set if there is no managed
  // PrefStore.
  scoped_ptr<PrefService> prefs = CreatePrefService(false);
  ProxyConfigDictionary dict(prefs->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict, ProxyPrefs::MODE_AUTO_DETECT);

  // Try a second time time with the managed PrefStore in place, the
  // auto-detect should be overridden. The default pref store must be
  // in place with the appropriate default value for this to work.
  prefs = CreatePrefService(true);
  ProxyConfigDictionary dict2(prefs->GetDictionary(prefs::kProxy));
  assertProxyModeWithoutParams(dict2, ProxyPrefs::MODE_DIRECT);
}

}  // namespace policy
