// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"

#include <stddef.h>

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class EncodingMenuControllerTest : public testing::Test {
 public:
  EncodingMenuControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}
 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
};

TEST_F(EncodingMenuControllerTest, EncodingIDsBelongTest) {
  EncodingMenuController controller;

  // Check some bogus ids to make sure they're never valid.
  ASSERT_FALSE(controller.DoesCommandBelongToEncodingMenu(0));
  ASSERT_FALSE(controller.DoesCommandBelongToEncodingMenu(-1));

  int num_valid_encoding_ids = controller.NumValidGUIEncodingIDs();
  const int* valid_encodings = controller.ValidGUIEncodingIDs();
  ASSERT_TRUE(controller.DoesCommandBelongToEncodingMenu(
      IDC_ENCODING_AUTO_DETECT));
  // Check that all valid encodings are accepted.
  for (int i = 0; i < num_valid_encoding_ids; ++i) {
    ASSERT_TRUE(controller.DoesCommandBelongToEncodingMenu(valid_encodings[i]));
  }

  // This test asserts that we haven't added a new valid ID without including it
  // in the kValidEncodingIds test list above.
  // The premise is that new encodings will be added directly after the current
  // ones so we'll catch such cases.
  int one_past_largest_id = valid_encodings[num_valid_encoding_ids - 1] + 1;
  ASSERT_FALSE(controller.DoesCommandBelongToEncodingMenu(one_past_largest_id));
}

TEST_F(EncodingMenuControllerTest, ListEncodingMenuItems) {
  typedef EncodingMenuController::EncodingMenuItemList EncodingMenuItemList;
  EncodingMenuController controller;

  EncodingMenuItemList english_items;
  TestingProfile profile_en;

  controller.GetEncodingMenuItems(&profile_en, &english_items);

  // Make sure there are items in the lists.
  ASSERT_FALSE(english_items.empty());
  // Make sure that autodetect is the first item on both menus
  ASSERT_EQ(english_items[0].first, IDC_ENCODING_AUTO_DETECT);
}

TEST_F(EncodingMenuControllerTest, IsItemChecked) {
  TestingProfile profile_en;
  std::string encoding("UTF-8");

  // Check that enabling and disabling autodetect works.
  bool autodetect_enabed[] = {true, false};
  PrefService* prefs = profile_en.GetPrefs();
  EncodingMenuController controller;
  for (size_t i = 0; i < arraysize(autodetect_enabed); ++i) {
    bool enabled = autodetect_enabed[i];
    prefs->SetBoolean(prefs::kWebKitUsesUniversalDetector, enabled);
    ASSERT_TRUE(controller.IsItemChecked(&profile_en,
                                         encoding,
                                         IDC_ENCODING_AUTO_DETECT) == enabled);
  }

  // Check all valid encodings, make sure only one is enabled when autodetection
  // is turned off.
  prefs->SetBoolean(prefs::kWebKitUsesUniversalDetector, false);
  bool encoding_is_enabled = false;
  size_t num_valid_encoding_ids = controller.NumValidGUIEncodingIDs();
  const int* valid_encodings = controller.ValidGUIEncodingIDs();
  for (size_t i = 0; i < num_valid_encoding_ids; ++i) {
    bool on = controller.IsItemChecked(&profile_en,
                                       encoding,
                                       valid_encodings[i]);
    // Only one item in the encoding menu can be selected at a time.
    ASSERT_FALSE(on && encoding_is_enabled);
    encoding_is_enabled |= on;
  }

  // Make sure at least one encoding is enabled.
  ASSERT_TRUE(encoding_is_enabled);
}
