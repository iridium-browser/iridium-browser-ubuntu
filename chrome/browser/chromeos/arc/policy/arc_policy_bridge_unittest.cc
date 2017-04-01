// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/policy/arc_policy_bridge.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/test/fake_policy_instance.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeONC[] =
    "{\"NetworkConfigurations\":["
    "{\"GUID\":\"{485d6076-dd44-6b6d-69787465725f5040}\","
    "\"Type\":\"WiFi\","
    "\"Name\":\"My WiFi Network\","
    "\"WiFi\":{"
    "\"HexSSID\":\"737369642D6E6F6E65\","  // "ssid-none"
    "\"Security\":\"None\"}"
    "}"
    "],"
    "\"GlobalNetworkConfiguration\":{"
    "\"AllowOnlyPolicyNetworksToAutoconnect\":true,"
    "},"
    "\"Certificates\":["
    "{ \"GUID\":\"{f998f760-272b-6939-4c2beffe428697ac}\","
    "\"PKCS12\":\"abc\","
    "\"Type\":\"Client\"},"
    "{\"Type\":\"Authority\","
    "\"TrustBits\":[\"Web\"],"
    "\"X509\":\"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ"
    "1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpc"
    "yBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCB"
    "pbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZ"
    "GdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4"
    "=\","
    "\"GUID\":\"{00f79111-51e0-e6e0-76b3b55450d80a1b}\"}"
    "]}";

constexpr char kPolicyCompliantResponse[] = "{ \"policyCompliant\": true }";
constexpr char kPolicyNonCompliantResponse[] = "{ \"policyCompliant\": false }";

// Helper class to define callbacks that verify that they were run.
// Wraps a bool initially set to |false| and verifies that it's been set to
// |true| before destruction.
class CheckedBoolean {
 public:
  CheckedBoolean() {}
  ~CheckedBoolean() { EXPECT_TRUE(value_); }

  void set_value(bool value) { value_ = value; }

 private:
  bool value_ = false;

  DISALLOW_COPY_AND_ASSIGN(CheckedBoolean);
};

void ExpectString(std::unique_ptr<CheckedBoolean> was_run,
                  const std::string& expected,
                  const std::string& received) {
  EXPECT_EQ(expected, received);
  was_run->set_value(true);
}

arc::ArcPolicyBridge::GetPoliciesCallback PolicyStringCallback(
    const std::string& expected) {
  std::unique_ptr<CheckedBoolean> was_run(new CheckedBoolean());
  return base::Bind(&ExpectString, base::Passed(&was_run), expected);
}

arc::ArcPolicyBridge::ReportComplianceCallback PolicyComplianceCallback(
    const std::string& expected) {
  std::unique_ptr<CheckedBoolean> was_run(new CheckedBoolean);
  return base::Bind(&ExpectString, base::Passed(&was_run), expected);
}

}  // namespace

using testing::_;
using testing::ReturnRef;

namespace arc {

class ArcPolicyBridgeTest : public testing::Test {
 public:
  ArcPolicyBridgeTest() {}

  void SetUp() override {
    bridge_service_ = base::MakeUnique<ArcBridgeService>();
    policy_bridge_ = base::MakeUnique<ArcPolicyBridge>(bridge_service_.get(),
                                                       &policy_service_);
    policy_bridge_->OverrideIsManagedForTesting(true);

    EXPECT_CALL(policy_service_,
                GetPolicies(policy::PolicyNamespace(
                    policy::POLICY_DOMAIN_CHROME, std::string())))
        .WillRepeatedly(ReturnRef(policy_map_));
    EXPECT_CALL(policy_service_, AddObserver(policy::POLICY_DOMAIN_CHROME, _))
        .Times(1);

    policy_instance_ = base::MakeUnique<FakePolicyInstance>();
    bridge_service_->policy()->SetInstance(policy_instance_.get());
  }

 protected:
  ArcPolicyBridge* policy_bridge() { return policy_bridge_.get(); }
  FakePolicyInstance* policy_instance() { return policy_instance_.get(); }
  policy::PolicyMap& policy_map() { return policy_map_; }

 private:
  // Not an unused variable. Unit tests do not have a message loop by themselves
  // and mojo needs a message loop for communication.
  base::MessageLoop loop_;
  std::unique_ptr<ArcBridgeService> bridge_service_;
  std::unique_ptr<ArcPolicyBridge> policy_bridge_;
  // Always keep policy_instance_ below bridge_service_, so that
  // policy_instance_ is destructed first. It needs to remove itself as
  // observer.
  std::unique_ptr<FakePolicyInstance> policy_instance_;
  policy::PolicyMap policy_map_;
  policy::MockPolicyService policy_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcPolicyBridgeTest);
};

TEST_F(ArcPolicyBridgeTest, UnmanagedTest) {
  policy_bridge()->OverrideIsManagedForTesting(false);
  policy_bridge()->GetPolicies(PolicyStringCallback(""));
}

TEST_F(ArcPolicyBridgeTest, EmptyPolicyTest) {
  // No policy is set, result should be empty.
  policy_bridge()->GetPolicies(PolicyStringCallback("{}"));
}

TEST_F(ArcPolicyBridgeTest, ArcPolicyTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::MakeUnique<base::StringValue>(
          "{\"applications\":"
          "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
          "\"installType\":\"REQUIRED\","
          "\"lockTaskAllowed\":false,"
          "\"permissionGrants\":[]"
          "}],"
          "\"defaultPermissionPolicy\":\"GRANT\""
          "}"),
      nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback(
      "{\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"lockTaskAllowed\":false,"
      "\"packageName\":\"com.google.android.apps.youtube.kids\","
      "\"permissionGrants\":[]"
      "}],"
      "\"defaultPermissionPolicy\":\"GRANT\""
      "}"));
}

TEST_F(ArcPolicyBridgeTest, HompageLocationTest) {
  // This policy will not be passed on, result should be empty.
  policy_map().Set(
      policy::key::kHomepageLocation, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::MakeUnique<base::StringValue>("http://chromium.org"), nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback("{}"));
}

TEST_F(ArcPolicyBridgeTest, DisableScreenshotsTest) {
  policy_map().Set(policy::key::kDisableScreenshots,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::FundamentalValue>(true), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"screenCaptureDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, VideoCaptureAllowedTest) {
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::FundamentalValue>(false), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"cameraDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, AudioCaptureAllowedTest) {
  policy_map().Set(policy::key::kAudioCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::FundamentalValue>(false), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"unmuteMicrophoneDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, DefaultGeolocationSettingTest) {
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::FundamentalValue>(1), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"shareLocationDisabled\":false}"));
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::FundamentalValue>(2), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"shareLocationDisabled\":true}"));
  policy_map().Set(policy::key::kDefaultGeolocationSetting,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::FundamentalValue>(3), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"shareLocationDisabled\":false}"));
}

TEST_F(ArcPolicyBridgeTest, ExternalStorageDisabledTest) {
  policy_map().Set(policy::key::kExternalStorageDisabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::FundamentalValue>(true), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"mountPhysicalMediaDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, URLBlacklistTest) {
  base::ListValue blacklist;
  blacklist.AppendString("www.blacklist1.com");
  blacklist.AppendString("www.blacklist2.com");
  policy_map().Set(policy::key::kURLBlacklist, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   blacklist.CreateDeepCopy(), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"globalAppRestrictions\":"
                           "{\"com.android.browser:URLBlacklist\":"
                           "[\"www.blacklist1.com\","
                           "\"www.blacklist2.com\""
                           "]}}"));
}

TEST_F(ArcPolicyBridgeTest, URLWhitelistTest) {
  base::ListValue whitelist;
  whitelist.AppendString("www.whitelist1.com");
  whitelist.AppendString("www.whitelist2.com");
  policy_map().Set(policy::key::kURLWhitelist, policy::POLICY_LEVEL_MANDATORY,
                   policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
                   whitelist.CreateDeepCopy(), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"globalAppRestrictions\":"
                           "{\"com.android.browser:URLWhitelist\":"
                           "[\"www.whitelist1.com\","
                           "\"www.whitelist2.com\""
                           "]}}"));
}

TEST_F(ArcPolicyBridgeTest, CaCertificateTest) {
  // Enable CA certificates sync.
  policy_map().Set(
      policy::key::kArcCertificatesSyncMode, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::MakeUnique<base::FundamentalValue>(ArcCertsSyncMode::COPY_CA_CERTS),
      nullptr);
  policy_map().Set(policy::key::kOpenNetworkConfiguration,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::StringValue>(kFakeONC), nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback(
      "{\"caCerts\":"
      "[{\"X509\":\"TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24"
      "sIGJ1dCBieSB0aGlzIHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGl"
      "jaCBpcyBhIGx1c3Qgb2YgdGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGV"
      "saWdodCBpbiB0aGUgY29udGludWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Y"
      "ga25vd2xlZGdlLCBleGNlZWRzIHRoZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCB"
      "wbGVhc3VyZS4=\"}"
      "]}"));

  // Disable CA certificates sync.
  policy_map().Set(
      policy::key::kArcCertificatesSyncMode, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::MakeUnique<base::FundamentalValue>(ArcCertsSyncMode::SYNC_DISABLED),
      nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback("{}"));
}

TEST_F(ArcPolicyBridgeTest, DeveloperToolsDisabledTest) {
  policy_map().Set(policy::key::kDeveloperToolsDisabled,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::FundamentalValue>(true), nullptr);
  policy_bridge()->GetPolicies(
      PolicyStringCallback("{\"debuggingFeaturesDisabled\":true}"));
}

TEST_F(ArcPolicyBridgeTest, MultiplePoliciesTest) {
  policy_map().Set(
      policy::key::kArcPolicy, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::MakeUnique<base::StringValue>(
          "{\"applications\":"
          "[{\"packageName\":\"com.google.android.apps.youtube.kids\","
          "\"installType\":\"REQUIRED\","
          "\"lockTaskAllowed\":false,"
          "\"permissionGrants\":[]"
          "}],"
          "\"defaultPermissionPolicy\":\"GRANT\"}"),
      nullptr);
  policy_map().Set(
      policy::key::kHomepageLocation, policy::POLICY_LEVEL_MANDATORY,
      policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
      base::MakeUnique<base::StringValue>("http://chromium.org"), nullptr);
  policy_map().Set(policy::key::kVideoCaptureAllowed,
                   policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
                   policy::POLICY_SOURCE_CLOUD,
                   base::MakeUnique<base::FundamentalValue>(false), nullptr);
  policy_bridge()->GetPolicies(PolicyStringCallback(
      "{\"applications\":"
      "[{\"installType\":\"REQUIRED\","
      "\"lockTaskAllowed\":false,"
      "\"packageName\":\"com.google.android.apps.youtube.kids\","
      "\"permissionGrants\":[]"
      "}],"
      "\"cameraDisabled\":true,"
      "\"defaultPermissionPolicy\":\"GRANT\""
      "}"));
}

// Disabled due to memory leak https://crbug.com/666371.
// TODO(poromov): Fix leak and re-enable.
TEST_F(ArcPolicyBridgeTest, DISABLED_EmptyReportComplianceTest) {
  policy_bridge()->ReportCompliance(
      "", PolicyComplianceCallback(kPolicyCompliantResponse));
}

// Disabled due to memory leak https://crbug.com/666371.
// TODO(poromov): Fix leak and re-enable.
TEST_F(ArcPolicyBridgeTest, DISABLED_ParsableReportComplianceTest) {
  policy_bridge()->ReportCompliance(
      "{\"nonComplianceDetails\" : []}",
      PolicyComplianceCallback(kPolicyCompliantResponse));
}

// Disabled due to memory leak https://crbug.com/666371.
// TODO(poromov): Fix leak and re-enable.
TEST_F(ArcPolicyBridgeTest, DISABLED_NonParsableReportComplianceTest) {
  policy_bridge()->ReportCompliance(
      "\"nonComplianceDetails\" : [}",
      PolicyComplianceCallback(kPolicyNonCompliantResponse));
}

// This and the following test send the policies through a mojo connection
// between a PolicyInstance and the PolicyBridge.
TEST_F(ArcPolicyBridgeTest, PolicyInstanceUnmanagedTest) {
  policy_bridge()->OverrideIsManagedForTesting(false);
  policy_instance()->CallGetPolicies(PolicyStringCallback(""));
}

TEST_F(ArcPolicyBridgeTest, PolicyInstanceManagedTest) {
  policy_instance()->CallGetPolicies(PolicyStringCallback("{}"));
}

}  // namespace arc
