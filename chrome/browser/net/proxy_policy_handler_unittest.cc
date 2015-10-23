// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/net/proxy_policy_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/policy/core/browser/configuration_policy_pref_store_test.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_prefs.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Test cases for the proxy policy settings.
class ProxyPolicyHandlerTest
    : public ConfigurationPolicyPrefStoreTest {
 public:
  void SetUp() override {
    ConfigurationPolicyPrefStoreTest::SetUp();
    handler_list_.AddHandler(
        make_scoped_ptr<ConfigurationPolicyHandler>(new ProxyPolicyHandler));
    // Reset the PolicyServiceImpl to one that has the policy fixup
    // preprocessor. The previous store must be nulled out first so that it
    // removes itself from the service's observer list.
    store_ = NULL;
    policy_service_.reset(new PolicyServiceImpl(providers_));
    store_ = new ConfigurationPolicyPrefStore(
        policy_service_.get(), &handler_list_, POLICY_LEVEL_MANDATORY);
  }

 protected:
  // Verify that all the proxy prefs are set to the specified expected values.
  void VerifyProxyPrefs(
      const std::string& expected_proxy_server,
      const std::string& expected_proxy_pac_url,
      const std::string& expected_proxy_bypass_list,
      const ProxyPrefs::ProxyMode& expected_proxy_mode) {
    const base::Value* value = NULL;
    ASSERT_TRUE(store_->GetValue(prefs::kProxy, &value));
    ASSERT_EQ(base::Value::TYPE_DICTIONARY, value->GetType());
    ProxyConfigDictionary dict(
        static_cast<const base::DictionaryValue*>(value));
    std::string s;
    if (expected_proxy_server.empty()) {
      EXPECT_FALSE(dict.GetProxyServer(&s));
    } else {
      ASSERT_TRUE(dict.GetProxyServer(&s));
      EXPECT_EQ(expected_proxy_server, s);
    }
    if (expected_proxy_pac_url.empty()) {
      EXPECT_FALSE(dict.GetPacUrl(&s));
    } else {
      ASSERT_TRUE(dict.GetPacUrl(&s));
      EXPECT_EQ(expected_proxy_pac_url, s);
    }
    if (expected_proxy_bypass_list.empty()) {
      EXPECT_FALSE(dict.GetBypassList(&s));
    } else {
      ASSERT_TRUE(dict.GetBypassList(&s));
      EXPECT_EQ(expected_proxy_bypass_list, s);
    }
    ProxyPrefs::ProxyMode mode;
    ASSERT_TRUE(dict.GetMode(&mode));
    EXPECT_EQ(expected_proxy_mode, mode);
  }
};

TEST_F(ProxyPolicyHandlerTest, ManualOptions) {
  PolicyMap policy;
  policy.Set(key::kProxyBypassList,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("http://chromium.org/override"),
             NULL);
  policy.Set(key::kProxyServer,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("chromium.org"),
             NULL);
  policy.Set(
      key::kProxyServerMode,
      POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER,
      new base::FundamentalValue(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE),
      NULL);
  UpdateProviderPolicy(policy);

  VerifyProxyPrefs("chromium.org",
                   std::string(),
                   "http://chromium.org/override",
                   ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ProxyPolicyHandlerTest, ManualOptionsReversedApplyOrder) {
  PolicyMap policy;
  policy.Set(
      key::kProxyServerMode,
      POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER,
      new base::FundamentalValue(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE),
      NULL);
  policy.Set(key::kProxyBypassList,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("http://chromium.org/override"),
             NULL);
  policy.Set(key::kProxyServer,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("chromium.org"),
             NULL);
  UpdateProviderPolicy(policy);

  VerifyProxyPrefs("chromium.org",
                   std::string(),
                   "http://chromium.org/override",
                   ProxyPrefs::MODE_FIXED_SERVERS);
}

TEST_F(ProxyPolicyHandlerTest, ManualOptionsInvalid) {
  PolicyMap policy;
  policy.Set(
      key::kProxyServerMode,
      POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_USER,
      new base::FundamentalValue(
          ProxyPolicyHandler::PROXY_MANUALLY_CONFIGURED_PROXY_SERVER_MODE),
      NULL);
  UpdateProviderPolicy(policy);

  const base::Value* value = NULL;
  EXPECT_FALSE(store_->GetValue(prefs::kProxy, &value));
}

TEST_F(ProxyPolicyHandlerTest, NoProxyServerMode) {
  PolicyMap policy;
  policy.Set(key::kProxyServerMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(ProxyPolicyHandler::PROXY_SERVER_MODE),
             NULL);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(
      std::string(), std::string(), std::string(), ProxyPrefs::MODE_DIRECT);
}

TEST_F(ProxyPolicyHandlerTest, NoProxyModeName) {
  PolicyMap policy;
  policy.Set(key::kProxyMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(ProxyPrefs::kDirectProxyModeName),
             NULL);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(
      std::string(), std::string(), std::string(), ProxyPrefs::MODE_DIRECT);
}

TEST_F(ProxyPolicyHandlerTest, AutoDetectProxyServerMode) {
  PolicyMap policy;
  policy.Set(key::kProxyServerMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(
                 ProxyPolicyHandler::PROXY_AUTO_DETECT_PROXY_SERVER_MODE),
             NULL);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   std::string(),
                   std::string(),
                   ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyHandlerTest, AutoDetectProxyModeName) {
  PolicyMap policy;
  policy.Set(key::kProxyMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(ProxyPrefs::kAutoDetectProxyModeName),
             NULL);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   std::string(),
                   std::string(),
                   ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyHandlerTest, PacScriptProxyMode) {
  PolicyMap policy;
  policy.Set(key::kProxyPacUrl,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("http://short.org/proxy.pac"),
             NULL);
  policy.Set(key::kProxyMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(ProxyPrefs::kPacScriptProxyModeName),
             NULL);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   "http://short.org/proxy.pac",
                   std::string(),
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ProxyPolicyHandlerTest, PacScriptProxyModeInvalid) {
  PolicyMap policy;
  policy.Set(key::kProxyMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(ProxyPrefs::kPacScriptProxyModeName),
             NULL);
  UpdateProviderPolicy(policy);
  const base::Value* value = NULL;
  EXPECT_FALSE(store_->GetValue(prefs::kProxy, &value));
}

// Regression test for http://crbug.com/78016, CPanel returns empty strings
// for unset properties.
TEST_F(ProxyPolicyHandlerTest, PacScriptProxyModeBug78016) {
  PolicyMap policy;
  policy.Set(key::kProxyServer,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(std::string()),
             NULL);
  policy.Set(key::kProxyPacUrl,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("http://short.org/proxy.pac"),
             NULL);
  policy.Set(key::kProxyMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(ProxyPrefs::kPacScriptProxyModeName),
             NULL);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   "http://short.org/proxy.pac",
                   std::string(),
                   ProxyPrefs::MODE_PAC_SCRIPT);
}

TEST_F(ProxyPolicyHandlerTest, UseSystemProxyServerMode) {
  PolicyMap policy;
  policy.Set(key::kProxyServerMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(
                 ProxyPolicyHandler::PROXY_USE_SYSTEM_PROXY_SERVER_MODE),
             NULL);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(
      std::string(), std::string(), std::string(), ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ProxyPolicyHandlerTest, UseSystemProxyMode) {
  PolicyMap policy;
  policy.Set(key::kProxyMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(ProxyPrefs::kSystemProxyModeName),
             NULL);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(
      std::string(), std::string(), std::string(), ProxyPrefs::MODE_SYSTEM);
}

TEST_F(ProxyPolicyHandlerTest,
       ProxyModeOverridesProxyServerMode) {
  PolicyMap policy;
  policy.Set(key::kProxyServerMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::FundamentalValue(ProxyPolicyHandler::PROXY_SERVER_MODE),
             NULL);
  policy.Set(key::kProxyMode,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue(ProxyPrefs::kAutoDetectProxyModeName),
             NULL);
  UpdateProviderPolicy(policy);
  VerifyProxyPrefs(std::string(),
                   std::string(),
                   std::string(),
                   ProxyPrefs::MODE_AUTO_DETECT);
}

TEST_F(ProxyPolicyHandlerTest, ProxyInvalid) {
  // No mode expects all three parameters being set.
  PolicyMap policy;
  policy.Set(key::kProxyPacUrl,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("http://short.org/proxy.pac"),
             NULL);
  policy.Set(key::kProxyBypassList,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("http://chromium.org/override"),
             NULL);
  policy.Set(key::kProxyServer,
             POLICY_LEVEL_MANDATORY,
             POLICY_SCOPE_USER,
             new base::StringValue("chromium.org"),
             NULL);
  for (int i = 0; i < ProxyPolicyHandler::MODE_COUNT; ++i) {
    policy.Set(key::kProxyServerMode,
               POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               new base::FundamentalValue(i),
               NULL);
    UpdateProviderPolicy(policy);
    const base::Value* value = NULL;
    EXPECT_FALSE(store_->GetValue(prefs::kProxy, &value));
  }
}

}  // namespace policy
