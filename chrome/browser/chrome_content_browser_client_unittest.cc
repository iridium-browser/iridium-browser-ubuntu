// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include <map>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome {

using ChromeContentBrowserClientTest = testing::Test;

TEST_F(ChromeContentBrowserClientTest, ShouldAssignSiteForURL) {
  ChromeContentBrowserClient client;
  EXPECT_FALSE(client.ShouldAssignSiteForURL(GURL("chrome-native://test")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("http://www.google.com")));
  EXPECT_TRUE(client.ShouldAssignSiteForURL(GURL("https://www.google.com")));
}

// BrowserWithTestWindowTest doesn't work on iOS and Android.
#if !defined(OS_ANDROID) && !defined(OS_IOS)

using ChromeContentBrowserClientWindowTest = BrowserWithTestWindowTest;

static void DidOpenURLForWindowTest(content::WebContents** target_contents,
                                    content::WebContents* opened_contents) {
  DCHECK(target_contents);

  *target_contents = opened_contents;
}

// This test opens two URLs using ContentBrowserClient::OpenURL. It expects the
// URLs to be opened in new tabs and activated, changing the active tabs after
// each call and increasing the tab count by 2.
TEST_F(ChromeContentBrowserClientWindowTest, OpenURL) {
  ChromeContentBrowserClient client;

  int previous_count = browser()->tab_strip_model()->count();

  GURL urls[] = { GURL("https://www.google.com"),
                  GURL("https://www.chromium.org") };

  for (const GURL& url : urls) {
    content::OpenURLParams params(url,
                                  content::Referrer(),
                                  NEW_FOREGROUND_TAB,
                                  ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                  false);
    // TODO(peter): We should have more in-depth browser tests for the window
    // opening functionality, which also covers Android. This test can currently
    // only be ran on platforms where OpenURL is implemented synchronously.
    // See https://crbug.com/457667.
    content::WebContents* web_contents = nullptr;
    client.OpenURL(browser()->profile(),
                   params,
                   base::Bind(&DidOpenURLForWindowTest, &web_contents));

    EXPECT_TRUE(web_contents);

    content::WebContents* active_contents = browser()->tab_strip_model()->
        GetActiveWebContents();
    EXPECT_EQ(web_contents, active_contents);
    EXPECT_EQ(url, active_contents->GetVisibleURL());
  }

  EXPECT_EQ(previous_count + 2, browser()->tab_strip_model()->count());
}

#endif // !defined(OS_ANDROID) && !defined(OS_IOS)

#if defined(ENABLE_WEBRTC)

// NOTE: Any updates to the expectations in these tests should also be done in
// the browser test WebRtcDisableEncryptionFlagBrowserTest.
class DisableWebRtcEncryptionFlagTest : public testing::Test {
 public:
  DisableWebRtcEncryptionFlagTest()
      : from_command_line_(base::CommandLine::NO_PROGRAM),
        to_command_line_(base::CommandLine::NO_PROGRAM) {}

 protected:
  void SetUp() override {
    from_command_line_.AppendSwitch(switches::kDisableWebRtcEncryption);
  }

  void MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel channel) {
    ChromeContentBrowserClient::MaybeCopyDisableWebRtcEncryptionSwitch(
        &to_command_line_,
        from_command_line_,
        channel);
  }

  base::CommandLine from_command_line_;
  base::CommandLine to_command_line_;

  DISALLOW_COPY_AND_ASSIGN(DisableWebRtcEncryptionFlagTest);
};

TEST_F(DisableWebRtcEncryptionFlagTest, UnknownChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::UNKNOWN);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, CanaryChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::CANARY);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, DevChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::DEV);
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

TEST_F(DisableWebRtcEncryptionFlagTest, BetaChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::BETA);
#if defined(OS_ANDROID)
  EXPECT_TRUE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
#else
  EXPECT_FALSE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
#endif
}

TEST_F(DisableWebRtcEncryptionFlagTest, StableChannel) {
  MaybeCopyDisableWebRtcEncryptionSwitch(version_info::Channel::STABLE);
  EXPECT_FALSE(to_command_line_.HasSwitch(switches::kDisableWebRtcEncryption));
}

#endif  // ENABLE_WEBRTC

class BlinkSettingsFieldTrialTest : public testing::Test {
 public:
  static const char kParserFieldTrialName[];
  static const char kIFrameFieldTrialName[];
  static const char kResourcePrioritiesFieldTrialName[];
  static const char kFakeGroupName[];
  static const char kDefaultGroupName[];

  BlinkSettingsFieldTrialTest()
      : trial_list_(NULL),
        command_line_(base::CommandLine::NO_PROGRAM) {}

  void SetUp() override {
    command_line_.AppendSwitchASCII(
        switches::kProcessType, switches::kRendererProcess);
  }

  void TearDown() override {
    variations::testing::ClearAllVariationParams();
  }

  void CreateFieldTrial(const char* trial_name, const char* group_name) {
    base::FieldTrialList::CreateFieldTrial(trial_name, group_name);
  }

  void CreateFieldTrialWithParams(
      const char* trial_name,
      const char* group_name,
      const char* key1, const char* value1,
      const char* key2, const char* value2) {
    std::map<std::string, std::string> params;
    params.insert(std::make_pair(key1, value1));
    params.insert(std::make_pair(key2, value2));
    CreateFieldTrial(trial_name, kFakeGroupName);
    variations::AssociateVariationParams(trial_name, kFakeGroupName, params);
  }

  void AppendContentBrowserClientSwitches() {
    client_.AppendExtraCommandLineSwitches(&command_line_, kFakeChildProcessId);
  }

  const base::CommandLine& command_line() const {
    return command_line_;
  }

  void AppendBlinkSettingsSwitch(const char* value) {
    command_line_.AppendSwitchASCII(switches::kBlinkSettings, value);
  }

 private:
  static const int kFakeChildProcessId = 1;

  ChromeContentBrowserClient client_;
  base::FieldTrialList trial_list_;
  base::CommandLine command_line_;

  content::TestBrowserThreadBundle thread_bundle_;
};

const char BlinkSettingsFieldTrialTest::kParserFieldTrialName[] =
    "BackgroundHtmlParserTokenLimits";
const char BlinkSettingsFieldTrialTest::kIFrameFieldTrialName[] =
    "LowPriorityIFrames";
const char BlinkSettingsFieldTrialTest::kResourcePrioritiesFieldTrialName[] =
    "ResourcePriorities";
const char BlinkSettingsFieldTrialTest::kFakeGroupName[] = "FakeGroup";
const char BlinkSettingsFieldTrialTest::kDefaultGroupName[] = "Default";

TEST_F(BlinkSettingsFieldTrialTest, NoFieldTrial) {
  AppendContentBrowserClientSwitches();
  EXPECT_FALSE(command_line().HasSwitch(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, FieldTrialWithoutParams) {
  CreateFieldTrial(kParserFieldTrialName, kFakeGroupName);
  AppendContentBrowserClientSwitches();
  EXPECT_FALSE(command_line().HasSwitch(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, BlinkSettingsSwitchAlreadySpecified) {
  AppendBlinkSettingsSwitch("foo");
  CreateFieldTrialWithParams(kParserFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("foo",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, FieldTrialEnabled) {
  CreateFieldTrialWithParams(kParserFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("key1=value1,key2=value2",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, MultipleFieldTrialsEnabled) {
  CreateFieldTrialWithParams(kParserFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  CreateFieldTrialWithParams(kIFrameFieldTrialName, kFakeGroupName,
                             "keyA", "valueA", "keyB", "valueB");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("key1=value1,key2=value2,keyA=valueA,keyB=valueB",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, MultipleFieldTrialsDuplicateKeys) {
  CreateFieldTrialWithParams(kParserFieldTrialName, kFakeGroupName,
                             "key1", "value1", "key2", "value2");
  CreateFieldTrialWithParams(kIFrameFieldTrialName, kFakeGroupName,
                             "key2", "duplicate", "key3", "value3");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("key1=value1,key2=value2,key2=duplicate,key3=value3",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, ResourcePrioritiesDefault) {
  CreateFieldTrial(kResourcePrioritiesFieldTrialName, kDefaultGroupName);
  AppendContentBrowserClientSwitches();
  EXPECT_FALSE(command_line().HasSwitch(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, ResourcePrioritiesEverythingEnabled) {
  CreateFieldTrial(kResourcePrioritiesFieldTrialName,
                   "Everything_11111_1_1_10");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("fetchDeferLateScripts=true,"
            "fetchIncreaseFontPriority=true,"
            "fetchIncreaseAsyncScriptPriority=true,"
            "fetchIncreasePriorities=true",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, ResourcePrioritiesDeferLateScripts) {
  CreateFieldTrial(kResourcePrioritiesFieldTrialName,
                   "LateScripts_10000_0_1_10");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("fetchDeferLateScripts=true",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, ResourcePrioritiesFontsEnabled) {
  CreateFieldTrial(kResourcePrioritiesFieldTrialName, "FontOnly_01000_0_1_10");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("fetchIncreaseFontPriority=true",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, ResourcePrioritiesIncreaseAsyncScript) {
  CreateFieldTrial(kResourcePrioritiesFieldTrialName,
                   "AsyncScript_00100_0_1_10");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("fetchIncreaseAsyncScriptPriority=true",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

TEST_F(BlinkSettingsFieldTrialTest, ResourcePrioritiesIncreasePriorities) {
  CreateFieldTrial(kResourcePrioritiesFieldTrialName,
                   "IncreasePriorities_00010_0_1_10");
  AppendContentBrowserClientSwitches();
  EXPECT_TRUE(command_line().HasSwitch(switches::kBlinkSettings));
  EXPECT_EQ("fetchIncreasePriorities=true",
            command_line().GetSwitchValueASCII(switches::kBlinkSettings));
}

}  // namespace chrome

#if !defined(OS_IOS) && !defined(OS_ANDROID)
namespace content {

class InstantNTPURLRewriteTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("42")));
  }

  void InstallTemplateURLWithNewTabPage(GURL new_tab_page_url) {
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), &TemplateURLServiceFactory::BuildInstanceFor);
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    ui_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("foo.com"));
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    data.new_tab_url = new_tab_page_url.spec();
    TemplateURL* template_url = new TemplateURL(data);
    // Takes ownership.
    template_url_service->Add(template_url);
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

TEST_F(InstantNTPURLRewriteTest, UberURLHandler_InstantExtendedNewTabPage) {
  const GURL url_original("chrome://newtab");
  const GURL url_rewritten("https://www.example.com/newtab");
  InstallTemplateURLWithNewTabPage(url_rewritten);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial("InstantExtended",
      "Group1 use_cacheable_ntp:1"));

  AddTab(browser(), GURL("chrome://blank"));
  NavigateAndCommitActiveTab(url_original);

  NavigationEntry* entry = browser()->tab_strip_model()->
      GetActiveWebContents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ(url_rewritten, entry->GetURL());
  EXPECT_EQ(url_original, entry->GetVirtualURL());
}

}  // namespace content
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)
