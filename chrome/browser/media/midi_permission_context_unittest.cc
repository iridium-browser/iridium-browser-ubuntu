// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_permission_context.h"

#include "base/bind.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_queue_controller.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestPermissionContext : public MidiPermissionContext {
 public:
  explicit TestPermissionContext(Profile* profile)
   : MidiPermissionContext(profile),
     permission_set_(false),
     permission_granted_(false),
     tab_context_updated_(false) {}

  ~TestPermissionContext() override {}

  PermissionQueueController* GetInfoBarController() {
    return GetQueueController();
  }

  bool permission_granted() {
    return permission_granted_;
  }

  bool permission_set() {
    return permission_set_;
  }

  bool tab_context_updated() {
    return tab_context_updated_;
  }

  void TrackPermissionDecision(ContentSetting content_setting) {
    permission_set_ = true;
    permission_granted_ = content_setting == CONTENT_SETTING_ALLOW;
  }

 protected:
  void UpdateTabContext(const PermissionRequestID& id,
                        const GURL& requesting_origin,
                        bool allowed) override {
    tab_context_updated_ = true;
  }

 private:
   bool permission_set_;
   bool permission_granted_;
   bool tab_context_updated_;
};

}  // anonymous namespace

class MidiPermissionContextTests : public ChromeRenderViewHostTestHarness {
 protected:
  MidiPermissionContextTests() = default;

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    InfoBarService::CreateForWebContents(web_contents());
    PermissionBubbleManager::CreateForWebContents(web_contents());
  }

  DISALLOW_COPY_AND_ASSIGN(MidiPermissionContextTests);
};

// Web MIDI permission should be denied for insecure origin.
TEST_F(MidiPermissionContextTests, TestInsecureRequestingUrl) {
  TestPermissionContext permission_context(profile());
  GURL url("http://www.example.com");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  const PermissionRequestID id(
      web_contents()->GetRenderProcessHost()->GetID(),
      web_contents()->GetMainFrame()->GetRoutingID(),
      -1, GURL());
  permission_context.RequestPermission(
      web_contents(),
      id, url, true,
      base::Bind(&TestPermissionContext::TrackPermissionDecision,
                 base::Unretained(&permission_context)));

  EXPECT_TRUE(permission_context.permission_set());
  EXPECT_FALSE(permission_context.permission_granted());
  EXPECT_TRUE(permission_context.tab_context_updated());

  ContentSetting setting =
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          url.GetOrigin(), url.GetOrigin(),
          CONTENT_SETTINGS_TYPE_MIDI_SYSEX, std::string());
  EXPECT_EQ(CONTENT_SETTING_ASK, setting);
}

// Web MIDI permission status should be denied for insecure origin.
TEST_F(MidiPermissionContextTests, TestInsecureQueryingUrl) {
  TestPermissionContext permission_context(profile());
  GURL insecure_url("http://www.example.com");
  GURL secure_url("https://www.example.com");

  // Check that there is no saved content settings.
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          insecure_url.GetOrigin(), insecure_url.GetOrigin(),
          CONTENT_SETTINGS_TYPE_MIDI_SYSEX, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          secure_url.GetOrigin(), insecure_url.GetOrigin(),
          CONTENT_SETTINGS_TYPE_MIDI_SYSEX, std::string()));
  EXPECT_EQ(CONTENT_SETTING_ASK,
      profile()->GetHostContentSettingsMap()->GetContentSetting(
          insecure_url.GetOrigin(), secure_url.GetOrigin(),
          CONTENT_SETTINGS_TYPE_MIDI_SYSEX, std::string()));

  EXPECT_EQ(CONTENT_SETTING_BLOCK, permission_context.GetPermissionStatus(
      insecure_url, insecure_url));
  EXPECT_EQ(CONTENT_SETTING_BLOCK, permission_context.GetPermissionStatus(
      insecure_url, secure_url));
}
