// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include <string>
#include <vector>

#include "ash/content/display/screen_orientation_controller_chromeos.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/message_center/message_center.h"

using ash::ResolutionNotificationController;

namespace chromeos {
namespace {
const char kPrimaryIdKey[] = "primary-id";
const char kMirroredKey[] = "mirrored";
const char kPositionKey[] = "position";
const char kOffsetKey[] = "offset";

// The mean acceleration due to gravity on Earth in m/s^2.
const float kMeanGravity = 9.80665f;

bool IsRotationLocked() {
  return ash::Shell::GetInstance()
      ->screen_orientation_controller()
      ->rotation_locked();
}

std::string ToPairString(const ash::DisplayIdPair& pair) {
  return base::Int64ToString(pair.first) + "," +
         base::Int64ToString(pair.second);
}

class DisplayPreferencesTest : public ash::test::AshTestBase {
 protected:
  DisplayPreferencesTest()
      : mock_user_manager_(new MockUserManager),
        user_manager_enabler_(mock_user_manager_) {
  }

  ~DisplayPreferencesTest() override {}

  void SetUp() override {
    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*mock_user_manager_, Shutdown());
    ash::test::AshTestBase::SetUp();
    RegisterDisplayLocalStatePrefs(local_state_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    observer_.reset(new DisplayConfigurationObserver());
  }

  void TearDown() override {
    observer_.reset();
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
    ash::test::AshTestBase::TearDown();
  }

  void LoggedInAsUser() {
    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsUserWithGaiaAccount())
        .WillRepeatedly(testing::Return(true));
  }

  void LoggedInAsGuest() {
    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsUserWithGaiaAccount())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsSupervisedUser())
        .WillRepeatedly(testing::Return(false));
  }

  // Do not use the implementation of display_preferences.cc directly to avoid
  // notifying the update to the system.
  void StoreDisplayLayoutPrefForPair(const ash::DisplayIdPair& pair,
                                     ash::DisplayLayout::Position layout,
                                     int offset,
                                     int64 primary_id) {
    std::string name = ToPairString(pair);
    DictionaryPrefUpdate update(&local_state_, prefs::kSecondaryDisplays);
    ash::DisplayLayout display_layout(layout, offset);
    display_layout.primary_id = primary_id;

    DCHECK(!name.empty());

    base::DictionaryValue* pref_data = update.Get();
    scoped_ptr<base::Value>layout_value(new base::DictionaryValue());
    if (pref_data->HasKey(name)) {
      base::Value* value = NULL;
      if (pref_data->Get(name, &value) && value != NULL)
        layout_value.reset(value->DeepCopy());
    }
    if (ash::DisplayLayout::ConvertToValue(display_layout, layout_value.get()))
      pref_data->Set(name, layout_value.release());
  }

  void StoreDisplayPropertyForPair(const ash::DisplayIdPair& pair,
                                   std::string key,
                                   scoped_ptr<base::Value> value) {
    std::string name = ToPairString(pair);

    DictionaryPrefUpdate update(&local_state_, prefs::kSecondaryDisplays);
    base::DictionaryValue* pref_data = update.Get();

    if (pref_data->HasKey(name)) {
      base::Value* layout_value = NULL;
      pref_data->Get(name, &layout_value);
      if (layout_value)
        static_cast<base::DictionaryValue*>(layout_value)
            ->Set(key, value.Pass());
    } else {
      scoped_ptr<base::DictionaryValue> layout_value(
          new base::DictionaryValue());
      layout_value->SetBoolean(key, value);
      pref_data->Set(name, layout_value.release());
    }
  }

  void StoreDisplayBoolPropertyForPair(const ash::DisplayIdPair& pair,
                                       const std::string& key,
                                       bool value) {
    StoreDisplayPropertyForPair(
        pair, key, make_scoped_ptr(new base::FundamentalValue(value)));
  }

  void StoreDisplayLayoutPrefForPair(const ash::DisplayIdPair& pair,
                                     ash::DisplayLayout::Position layout,
                                     int offset) {
    StoreDisplayLayoutPrefForPair(pair, layout, offset, pair.first);
  }

  void StoreDisplayOverscan(int64 id, const gfx::Insets& insets) {
    DictionaryPrefUpdate update(&local_state_, prefs::kDisplayProperties);
    const std::string name = base::Int64ToString(id);

    base::DictionaryValue* pref_data = update.Get();
    base::DictionaryValue* insets_value = new base::DictionaryValue();
    insets_value->SetInteger("insets_top", insets.top());
    insets_value->SetInteger("insets_left", insets.left());
    insets_value->SetInteger("insets_bottom", insets.bottom());
    insets_value->SetInteger("insets_right", insets.right());
    pref_data->Set(name, insets_value);
  }

  void StoreColorProfile(int64 id, const std::string& profile) {
    DictionaryPrefUpdate update(&local_state_, prefs::kDisplayProperties);
    const std::string name = base::Int64ToString(id);

    base::DictionaryValue* pref_data = update.Get();
    base::DictionaryValue* property = new base::DictionaryValue();
    property->SetString("color_profile_name", profile);
    pref_data->Set(name, property);
  }

  void StoreDisplayRotationPrefsForTest(bool rotation_lock,
                                        gfx::Display::Rotation rotation) {
    DictionaryPrefUpdate update(local_state(), prefs::kDisplayRotationLock);
    base::DictionaryValue* pref_data = update.Get();
    pref_data->SetBoolean("lock", rotation_lock);
    pref_data->SetInteger("orientation", static_cast<int>(rotation));
  }

  std::string GetRegisteredDisplayLayoutStr(const ash::DisplayIdPair& pair) {
    return ash::Shell::GetInstance()->display_manager()->layout_store()->
        GetRegisteredDisplayLayout(pair).ToString();
  }

  PrefService* local_state() { return &local_state_; }

 private:
  MockUserManager* mock_user_manager_;  // Not owned.
  ScopedUserManagerEnabler user_manager_enabler_;
  TestingPrefServiceSimple local_state_;
  scoped_ptr<DisplayConfigurationObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPreferencesTest);
};

}  // namespace

TEST_F(DisplayPreferencesTest, PairedLayoutOverrides) {
  UpdateDisplay("100x100,200x200");
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  ash::DisplayIdPair pair = display_manager->GetCurrentDisplayIdPair();
  ash::DisplayIdPair dummy_pair = std::make_pair(pair.first, pair.second + 1);
  ASSERT_NE(pair.first, dummy_pair.second);

  StoreDisplayLayoutPrefForPair(pair, ash::DisplayLayout::TOP, 20);
  StoreDisplayLayoutPrefForPair(dummy_pair, ash::DisplayLayout::LEFT, 30);
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);

  ash::Shell* shell = ash::Shell::GetInstance();

  LoadDisplayPreferences(true);
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            shell->display_configurator()->requested_power_state());

  shell->display_manager()->UpdateDisplays();
  // Check if the layout settings are notified to the system properly.
  // The paired layout overrides old layout.
  // Inverted one of for specified pair (id1, id2).  Not used for the pair
  // (id1, dummy_id) since dummy_id is not connected right now.
  EXPECT_EQ("top, 20",
            shell->display_manager()->GetCurrentDisplayLayout().ToString());
  EXPECT_EQ("top, 20", GetRegisteredDisplayLayoutStr(pair));
  EXPECT_EQ("left, 30", GetRegisteredDisplayLayoutStr(dummy_pair));
}

TEST_F(DisplayPreferencesTest, BasicStores) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200*2, 400x300#400x400|300x200*1.25");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  ash::test::DisplayManagerTestApi test_api(display_manager);
  test_api.SetInternalDisplayId(id1);
  int64 id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);
  std::vector<ui::ColorCalibrationProfile> profiles;
  profiles.push_back(ui::COLOR_PROFILE_STANDARD);
  profiles.push_back(ui::COLOR_PROFILE_DYNAMIC);
  profiles.push_back(ui::COLOR_PROFILE_MOVIE);
  profiles.push_back(ui::COLOR_PROFILE_READING);
  // Allows only |id1|.
  test_api.SetAvailableColorProfiles(id1, profiles);
  display_manager->SetColorCalibrationProfile(id1, ui::COLOR_PROFILE_DYNAMIC);
  display_manager->SetColorCalibrationProfile(id2, ui::COLOR_PROFILE_DYNAMIC);

  LoggedInAsUser();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentDisplayLayout(layout);
  StoreDisplayLayoutPrefForTest(
      id1, dummy_id, ash::DisplayLayout(ash::DisplayLayout::LEFT, 20));
  // Can't switch to a display that does not exist.
  display_controller->SetPrimaryDisplayId(dummy_id);
  EXPECT_NE(dummy_id, ash::Shell::GetScreen()->GetPrimaryDisplay().id());

  display_controller->SetOverscanInsets(id1, gfx::Insets(10, 11, 12, 13));
  display_manager->SetDisplayRotation(id1, gfx::Display::ROTATE_90,
                                      gfx::Display::ROTATION_SOURCE_USER);
  EXPECT_TRUE(display_manager->SetDisplayUIScale(id1, 1.25f));
  EXPECT_FALSE(display_manager->SetDisplayUIScale(id2, 1.25f));

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* layout_value = NULL;
  std::string key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));

  ash::DisplayLayout stored_layout;
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*layout_value,
                                                   &stored_layout));
  EXPECT_EQ(layout.position, stored_layout.position);
  EXPECT_EQ(layout.offset, stored_layout.offset);

  bool mirrored = true;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_FALSE(mirrored);

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = NULL;
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id1), &property));
  int ui_scale = 0;
  int rotation = 0;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_TRUE(property->GetInteger("ui-scale", &ui_scale));
  EXPECT_EQ(1, rotation);
  EXPECT_EQ(1250, ui_scale);

  // Internal display never registered the resolution.
  int width = 0, height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  int top = 0, left = 0, bottom = 0, right = 0;
  EXPECT_TRUE(property->GetInteger("insets_top", &top));
  EXPECT_TRUE(property->GetInteger("insets_left", &left));
  EXPECT_TRUE(property->GetInteger("insets_bottom", &bottom));
  EXPECT_TRUE(property->GetInteger("insets_right", &right));
  EXPECT_EQ(10, top);
  EXPECT_EQ(11, left);
  EXPECT_EQ(12, bottom);
  EXPECT_EQ(13, right);

  std::string color_profile;
  EXPECT_TRUE(property->GetString("color_profile_name", &color_profile));
  EXPECT_EQ("dynamic", color_profile);

  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_TRUE(property->GetInteger("ui-scale", &ui_scale));
  EXPECT_EQ(0, rotation);
  // ui_scale works only on 2x scale factor/1st display.
  EXPECT_EQ(1000, ui_scale);
  EXPECT_FALSE(property->GetInteger("insets_top", &top));
  EXPECT_FALSE(property->GetInteger("insets_left", &left));
  EXPECT_FALSE(property->GetInteger("insets_bottom", &bottom));
  EXPECT_FALSE(property->GetInteger("insets_right", &right));

  // |id2| doesn't have the color_profile because it doesn't have 'dynamic' in
  // its available list.
  EXPECT_FALSE(property->GetString("color_profile_name", &color_profile));

  // Resolution is saved only when the resolution is set
  // by DisplayManager::SetDisplayMode
  width = 0;
  height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  ash::DisplayMode mode(gfx::Size(300, 200), 60.0f, false, true);
  mode.device_scale_factor = 1.25f;
  display_manager->SetDisplayMode(id2, mode);

  display_controller->SetPrimaryDisplayId(id2);

  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id1), &property));
  width = 0;
  height = 0;
  // Internal display shouldn't store its resolution.
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // External display's resolution must be stored this time because
  // it's not best.
  int device_scale_factor = 0;
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_TRUE(property->GetInteger(
      "device-scale-factor", &device_scale_factor));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);
  EXPECT_EQ(1250, device_scale_factor);

  // The layout remains the same.
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*layout_value,
                                                   &stored_layout));
  EXPECT_EQ(layout.position, stored_layout.position);
  EXPECT_EQ(layout.offset, stored_layout.offset);
  EXPECT_EQ(id2, stored_layout.primary_id);

  mirrored = true;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_FALSE(mirrored);
  std::string primary_id_str;
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::Int64ToString(id2), primary_id_str);

  SetCurrentDisplayLayout(
      ash::DisplayLayout(ash::DisplayLayout::BOTTOM, 20));

  UpdateDisplay("1+0-200x200*2,1+0-200x200");
  // Mirrored.
  int offset = 0;
  std::string position;
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(layout_value->GetString(kPositionKey, &position));
  EXPECT_EQ("top", position);
  EXPECT_TRUE(layout_value->GetInteger(kOffsetKey, &offset));
  EXPECT_EQ(-20, offset);
  mirrored = false;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_TRUE(mirrored);
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::Int64ToString(id2), primary_id_str);

  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id1), &property));
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // External display's selected resolution must not change
  // by mirroring.
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);

  // Set new display's selected resolution.
  display_manager->RegisterDisplayProperty(
      id2 + 1, gfx::Display::ROTATE_0, 1.0f, NULL, gfx::Size(500, 400), 1.0f,
      ui::COLOR_PROFILE_STANDARD);

  UpdateDisplay("200x200*2, 600x500#600x500|500x400");

  // Update key as the 2nd display gets new id.
  id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(layout_value->GetString(kPositionKey, &position));
  EXPECT_EQ("right", position);
  EXPECT_TRUE(layout_value->GetInteger(kOffsetKey, &offset));
  EXPECT_EQ(0, offset);
  mirrored = true;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_FALSE(mirrored);
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::Int64ToString(id1), primary_id_str);

  // Best resolution should not be saved.
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // Set yet another new display's selected resolution.
  display_manager->RegisterDisplayProperty(
      id2 + 1, gfx::Display::ROTATE_0, 1.0f, NULL, gfx::Size(500, 400), 1.0f,
      ui::COLOR_PROFILE_STANDARD);
  // Disconnect 2nd display first to generate new id for external display.
  UpdateDisplay("200x200*2");
  UpdateDisplay("200x200*2, 500x400#600x500|500x400%60.0f");
  // Update key as the 2nd display gets new id.
  id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(layout_value->GetString(kPositionKey, &position));
  EXPECT_EQ("right", position);
  EXPECT_TRUE(layout_value->GetInteger(kOffsetKey, &offset));
  EXPECT_EQ(0, offset);
  mirrored = true;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_FALSE(mirrored);
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::Int64ToString(id1), primary_id_str);

  // External display's selected resolution must be updated.
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(500, width);
  EXPECT_EQ(400, height);
}

TEST_F(DisplayPreferencesTest, PreventStore) {
  ResolutionNotificationController::SuppressTimerForTest();
  LoggedInAsUser();
  UpdateDisplay("400x300#500x400|400x300|300x200");
  int64 id = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  // Set display's resolution in single display. It creates the notification and
  // display preferences should not stored meanwhile.
  ash::Shell* shell = ash::Shell::GetInstance();
  ash::DisplayMode old_mode;
  ash::DisplayMode new_mode;
  old_mode.size = gfx::Size(400, 300);
  new_mode.size = gfx::Size(500, 400);
  if (shell->display_manager()->SetDisplayMode(id, new_mode)) {
    shell->resolution_notification_controller()->PrepareNotification(
        id, old_mode, new_mode, base::Closure());
  }
  UpdateDisplay("500x400#500x400|400x300|300x200");

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = NULL;
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id), &property));
  int width = 0, height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // Revert the change. When timeout, 2nd button is revert.
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      ResolutionNotificationController::kNotificationId, 1);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          ResolutionNotificationController::kNotificationId));

  // Once the notification is removed, the specified resolution will be stored
  // by SetDisplayMode.
  ash::Shell::GetInstance()->display_manager()->SetDisplayMode(
      id, ash::DisplayMode(gfx::Size(300, 200), 60.0f, false, true));
  UpdateDisplay("300x200#500x400|400x300|300x200");

  property = NULL;
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);
}

TEST_F(DisplayPreferencesTest, StoreForSwappedDisplay) {
  UpdateDisplay("100x100,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenUtil::GetSecondaryDisplay().id();

  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  display_controller->SwapPrimaryDisplay();
  ASSERT_EQ(id1, ash::ScreenUtil::GetSecondaryDisplay().id());

  LoggedInAsUser();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentDisplayLayout(layout);
  layout = layout.Invert();

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* new_value = NULL;
  std::string key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &new_value));

  ash::DisplayLayout stored_layout;
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*new_value, &stored_layout));
  EXPECT_EQ(layout.position, stored_layout.position);
  EXPECT_EQ(layout.offset, stored_layout.offset);
  EXPECT_EQ(id2, stored_layout.primary_id);

  display_controller->SwapPrimaryDisplay();
  EXPECT_TRUE(displays->GetDictionary(key, &new_value));
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*new_value, &stored_layout));
  EXPECT_EQ(layout.position, stored_layout.position);
  EXPECT_EQ(layout.offset, stored_layout.offset);
  EXPECT_EQ(id1, stored_layout.primary_id);
}

TEST_F(DisplayPreferencesTest, RestoreColorProfiles) {
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();

  StoreColorProfile(id1, "dynamic");

  LoggedInAsUser();
  LoadDisplayPreferences(false);

  // id1's available color profiles list is empty, means somehow the color
  // profile suport is temporary in trouble.
  EXPECT_NE(ui::COLOR_PROFILE_DYNAMIC,
            display_manager->GetDisplayInfo(id1).color_profile());

  // Once the profile is supported, the color profile should be restored.
  std::vector<ui::ColorCalibrationProfile> profiles;
  profiles.push_back(ui::COLOR_PROFILE_STANDARD);
  profiles.push_back(ui::COLOR_PROFILE_DYNAMIC);
  profiles.push_back(ui::COLOR_PROFILE_MOVIE);
  profiles.push_back(ui::COLOR_PROFILE_READING);
  ash::test::DisplayManagerTestApi test_api(display_manager);
  test_api.SetAvailableColorProfiles(id1, profiles);

  LoadDisplayPreferences(false);
  EXPECT_EQ(ui::COLOR_PROFILE_DYNAMIC,
            display_manager->GetDisplayInfo(id1).color_profile());
}

TEST_F(DisplayPreferencesTest, DontStoreInGuestMode) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200*2,200x200");

  LoggedInAsGuest();
  int64 id1 = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  ash::test::DisplayManagerTestApi(display_manager)
      .SetInternalDisplayId(id1);
  int64 id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentDisplayLayout(layout);
  display_manager->SetDisplayUIScale(id1, 1.25f);
  display_controller->SetPrimaryDisplayId(id2);
  int64 new_primary = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  display_controller->SetOverscanInsets(
      new_primary,
      gfx::Insets(10, 11, 12, 13));
  display_manager->SetDisplayRotation(new_primary, gfx::Display::ROTATE_90,
                                      gfx::Display::ROTATION_SOURCE_USER);

  // Does not store the preferences locally.
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kSecondaryDisplays)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kDisplayProperties)->HasUserSetting());

  // Settings are still notified to the system.
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  EXPECT_EQ(id2, screen->GetPrimaryDisplay().id());
  EXPECT_EQ(ash::DisplayLayout::BOTTOM,
            display_manager->GetCurrentDisplayLayout().position);
  EXPECT_EQ(-10, display_manager->GetCurrentDisplayLayout().offset);
  const gfx::Display& primary_display = screen->GetPrimaryDisplay();
  EXPECT_EQ("178x176", primary_display.bounds().size().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_90, primary_display.rotation());

  const ash::DisplayInfo& info1 = display_manager->GetDisplayInfo(id1);
  EXPECT_EQ(1.25f, info1.configured_ui_scale());

  const ash::DisplayInfo& info_primary =
      display_manager->GetDisplayInfo(new_primary);
  EXPECT_EQ(gfx::Display::ROTATE_90, info_primary.GetActiveRotation());
  EXPECT_EQ(1.0f, info_primary.configured_ui_scale());
}

TEST_F(DisplayPreferencesTest, StorePowerStateNoLogin) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayPowerState));

  // Stores display prefs without login, which still stores the power state.
  StoreDisplayPrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayPowerState));
}

TEST_F(DisplayPreferencesTest, StorePowerStateGuest) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayPowerState));

  LoggedInAsGuest();
  StoreDisplayPrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayPowerState));
}

TEST_F(DisplayPreferencesTest, StorePowerStateNormalUser) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayPowerState));

  LoggedInAsUser();
  StoreDisplayPrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayPowerState));
}

TEST_F(DisplayPreferencesTest, DisplayPowerStateAfterRestart) {
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  LoadDisplayPreferences(false);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            ash::Shell::GetInstance()->display_configurator()->
                requested_power_state());
}

TEST_F(DisplayPreferencesTest, DontSaveAndRestoreAllOff) {
  ash::Shell* shell = ash::Shell::GetInstance();
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  LoadDisplayPreferences(false);
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            shell->display_configurator()->requested_power_state());

  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_ALL_OFF);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            shell->display_configurator()->requested_power_state());
  EXPECT_EQ("internal_off_external_on",
            local_state()->GetString(prefs::kDisplayPowerState));

  // Don't try to load
  local_state()->SetString(prefs::kDisplayPowerState, "all_off");
  LoadDisplayPreferences(false);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            shell->display_configurator()->requested_power_state());
}

// Tests that display configuration changes caused by MaximizeModeController
// are not saved.
TEST_F(DisplayPreferencesTest, DontSaveMaximizeModeControllerRotations) {
  ash::Shell* shell = ash::Shell::GetInstance();
  gfx::Display::SetInternalDisplayId(
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id());
  ash::DisplayManager* display_manager = shell->display_manager();
  LoggedInAsUser();
  // Populate the properties.
  display_manager->SetDisplayRotation(gfx::Display::InternalDisplayId(),
                                      gfx::Display::ROTATE_180,
                                      gfx::Display::ROTATION_SOURCE_USER);
  // Reset property to avoid rotation lock
  display_manager->SetDisplayRotation(gfx::Display::InternalDisplayId(),
                                      gfx::Display::ROTATE_0,
                                      gfx::Display::ROTATION_SOURCE_USER);

  // Open up 270 degrees to trigger maximize mode
  scoped_refptr<chromeos::AccelerometerUpdate> update(
      new chromeos::AccelerometerUpdate());
  update->Set(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, 0.0f, 0.0f,
             kMeanGravity);
  update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, 0.0f, -kMeanGravity, 0.0f);
  ash::MaximizeModeController* controller = shell->maximize_mode_controller();
  controller->OnAccelerometerUpdated(update);
  EXPECT_TRUE(controller->IsMaximizeModeWindowManagerEnabled());

  // Trigger 90 degree rotation
  update->Set(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, -kMeanGravity,
             0.0f, 0.0f);
  update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, -kMeanGravity, 0.0f, 0.0f);
  controller->OnAccelerometerUpdated(update);
  shell->screen_orientation_controller()->OnAccelerometerUpdated(update);
  EXPECT_EQ(gfx::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = NULL;
  EXPECT_TRUE(properties->GetDictionary(
      base::Int64ToString(gfx::Display::InternalDisplayId()), &property));
  int rotation = -1;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_EQ(gfx::Display::ROTATE_0, rotation);

  // Trigger a save, the acceleration rotation should not be saved as the user
  // rotation.
  StoreDisplayPrefs();
  properties = local_state()->GetDictionary(prefs::kDisplayProperties);
  property = NULL;
  EXPECT_TRUE(properties->GetDictionary(
      base::Int64ToString(gfx::Display::InternalDisplayId()), &property));
  rotation = -1;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_EQ(gfx::Display::ROTATE_0, rotation);
}

// Tests that the rotation state is saved without a user being logged in.
TEST_F(DisplayPreferencesTest, StoreRotationStateNoLogin) {
  gfx::Display::SetInternalDisplayId(
            gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  bool current_rotation_lock = IsRotationLocked();
  StoreDisplayRotationPrefs(current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
  EXPECT_EQ(current_rotation_lock, rotation_lock);

  int orientation;
  gfx::Display::Rotation current_rotation = GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(properties->GetInteger("orientation", &orientation));
  EXPECT_EQ(current_rotation, orientation);
}

// Tests that the rotation state is saved when a guest is logged in.
TEST_F(DisplayPreferencesTest, StoreRotationStateGuest) {
  gfx::Display::SetInternalDisplayId(
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));
  LoggedInAsGuest();

  bool current_rotation_lock = IsRotationLocked();
  StoreDisplayRotationPrefs(current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
  EXPECT_EQ(current_rotation_lock, rotation_lock);

  int orientation;
  gfx::Display::Rotation current_rotation = GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(properties->GetInteger("orientation", &orientation));
  EXPECT_EQ(current_rotation, orientation);
}

// Tests that the rotation state is saved when a normal user is logged in.
TEST_F(DisplayPreferencesTest, StoreRotationStateNormalUser) {
  gfx::Display::SetInternalDisplayId(
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));
  LoggedInAsGuest();

  bool current_rotation_lock = IsRotationLocked();
  StoreDisplayRotationPrefs(current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
  EXPECT_EQ(current_rotation_lock, rotation_lock);

  int orientation;
  gfx::Display::Rotation current_rotation = GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(properties->GetInteger("orientation", &orientation));
  EXPECT_EQ(current_rotation, orientation);
}

// Tests that rotation state is loaded without a user being logged in, and that
// entering maximize mode applies the state.
TEST_F(DisplayPreferencesTest, LoadRotationNoLogin) {
  gfx::Display::SetInternalDisplayId(
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id());
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  ash::Shell* shell = ash::Shell::GetInstance();
  bool initial_rotation_lock = IsRotationLocked();
  ASSERT_FALSE(initial_rotation_lock);
  ash::DisplayManager* display_manager = shell->display_manager();
  gfx::Display::Rotation initial_rotation = GetCurrentInternalDisplayRotation();
  ASSERT_EQ(gfx::Display::ROTATE_0, initial_rotation);

  StoreDisplayRotationPrefs(initial_rotation_lock);
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  StoreDisplayRotationPrefsForTest(true, gfx::Display::ROTATE_90);
  LoadDisplayPreferences(false);

  bool display_rotation_lock =
      display_manager->registered_internal_display_rotation_lock();
  bool display_rotation =
      display_manager->registered_internal_display_rotation();
  EXPECT_TRUE(display_rotation_lock);
  EXPECT_EQ(gfx::Display::ROTATE_90, display_rotation);

  bool rotation_lock = IsRotationLocked();
  gfx::Display::Rotation before_maximize_mode_rotation =
      GetCurrentInternalDisplayRotation();

  // Settings should not be applied until maximize mode activates
  EXPECT_FALSE(rotation_lock);
  EXPECT_EQ(gfx::Display::ROTATE_0, before_maximize_mode_rotation);

  // Open up 270 degrees to trigger maximize mode
  scoped_refptr<chromeos::AccelerometerUpdate> update(
      new chromeos::AccelerometerUpdate());
  update->Set(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, 0.0f, 0.0f,
             kMeanGravity);
  update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, 0.0f, -kMeanGravity, 0.0f);
  ash::MaximizeModeController* maximize_mode_controller =
      shell->maximize_mode_controller();
  maximize_mode_controller->OnAccelerometerUpdated(update);
  EXPECT_TRUE(maximize_mode_controller->IsMaximizeModeWindowManagerEnabled());
  bool screen_orientation_rotation_lock = IsRotationLocked();
  gfx::Display::Rotation maximize_mode_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(screen_orientation_rotation_lock);
  EXPECT_EQ(gfx::Display::ROTATE_90, maximize_mode_rotation);
}

// Tests that rotation lock being set causes the rotation state to be saved.
TEST_F(DisplayPreferencesTest, RotationLockTriggersStore) {
  gfx::Display::SetInternalDisplayId(
    gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id());
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  ash::Shell::GetInstance()->screen_orientation_controller()->SetRotationLocked(
      true);

  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
}

TEST_F(DisplayPreferencesTest, SaveUnifiedMode) {
  UpdateDisplay("100x100,200x200");
  LoggedInAsUser();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  ash::DisplayIdPair pair = display_manager->GetCurrentDisplayIdPair();
  std::string pair_key = ToPairString(pair);

  // Unified mode should be recorded.
  display_manager->SetDefaultMultiDisplayMode(ash::DisplayManager::UNIFIED);
  display_manager->ReconfigureDisplays();

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* new_value = NULL;
  EXPECT_TRUE(displays->GetDictionary(ToPairString(pair), &new_value));

  ash::DisplayLayout stored_layout;
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);
  EXPECT_FALSE(stored_layout.mirrored);

  // Mirror mode should remember if the default mode was unified.
  display_manager->SetMirrorMode(true);
  EXPECT_TRUE(displays->GetDictionary(ToPairString(pair), &new_value));
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);
  EXPECT_TRUE(stored_layout.mirrored);

  display_manager->SetMirrorMode(false);
  EXPECT_TRUE(displays->GetDictionary(ToPairString(pair), &new_value));
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);
  EXPECT_FALSE(stored_layout.mirrored);

  // Exit unified mode.
  display_manager->SetDefaultMultiDisplayMode(ash::DisplayManager::EXTENDED);
  display_manager->ReconfigureDisplays();
  EXPECT_TRUE(displays->GetDictionary(ToPairString(pair), &new_value));
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*new_value, &stored_layout));
  EXPECT_FALSE(stored_layout.default_unified);
  EXPECT_FALSE(stored_layout.mirrored);
}

TEST_F(DisplayPreferencesTest, RestoreUnifiedMode) {
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  ash::DisplayIdPair pair = std::make_pair(id1, id1 + 1);
  StoreDisplayBoolPropertyForPair(pair, "default_unified", true);
  StoreDisplayPropertyForPair(
      pair, "primary-id",
      make_scoped_ptr(new base::StringValue(base::Int64ToString(id1))));
  LoadDisplayPreferences(false);

  // Should not restore to unified unless unified desktop is enabled.
  UpdateDisplay("100x100,200x200");
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  EXPECT_FALSE(display_manager->IsInUnifiedMode());

  // Restored to unified.
  ash::test::DisplayManagerTestApi::EnableUnifiedDesktopForTest();
  StoreDisplayBoolPropertyForPair(pair, "default_unified", true);
  LoadDisplayPreferences(false);
  UpdateDisplay("100x100,200x200");
  EXPECT_TRUE(display_manager->IsInUnifiedMode());

  // Restored to mirror, then unified.
  StoreDisplayBoolPropertyForPair(pair, "mirrored", true);
  StoreDisplayBoolPropertyForPair(pair, "default_unified", true);
  LoadDisplayPreferences(false);
  UpdateDisplay("100x100,200x200");
  EXPECT_TRUE(display_manager->IsInMirrorMode());

  display_manager->SetMirrorMode(false);
  EXPECT_TRUE(display_manager->IsInUnifiedMode());

  // Sanity check. Restore to extended.
  StoreDisplayBoolPropertyForPair(pair, "default_unified", false);
  StoreDisplayBoolPropertyForPair(pair, "mirrored", false);
  LoadDisplayPreferences(false);
  UpdateDisplay("100x100,200x200");
  EXPECT_FALSE(display_manager->IsInMirrorMode());
  EXPECT_FALSE(display_manager->IsInUnifiedMode());
}

}  // namespace chromeos
