// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include <vector>

#include "ash/autoclick/autoclick_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/input_method/input_method_syncer.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/chromeos/net/wake_on_wifi_manager.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/feedback/tracing_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/chromeos/accessibility_types.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "url/gurl.h"

namespace chromeos {

static const char kFallbackInputMethodLocale[] = "en-US";

Preferences::Preferences()
    : prefs_(NULL),
      input_method_manager_(input_method::InputMethodManager::Get()),
      user_(NULL),
      user_is_primary_(false) {
  // Do not observe shell, if there is no shell instance; e.g., in some unit
  // tests.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->AddShellObserver(this);
}

Preferences::Preferences(input_method::InputMethodManager* input_method_manager)
    : prefs_(NULL),
      input_method_manager_(input_method_manager),
      user_(NULL),
      user_is_primary_(false) {
  // Do not observe shell, if there is no shell instance; e.g., in some unit
  // tests.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->AddShellObserver(this);
}

Preferences::~Preferences() {
  prefs_->RemoveObserver(this);
  user_manager::UserManager::Get()->RemoveSessionStateObserver(this);
  // If shell instance is destoryed before this preferences instance, there is
  // no need to remove this shell observer.
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->RemoveShellObserver(this);
}

// static
void Preferences::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kOwnerPrimaryMouseButtonRight, false);
  registry->RegisterBooleanPref(prefs::kOwnerTapToClickEnabled, true);
  registry->RegisterBooleanPref(prefs::kAccessibilityVirtualKeyboardEnabled,
                                false);
  registry->RegisterStringPref(prefs::kLogoutStartedLast, std::string());
  registry->RegisterBooleanPref(prefs::kResolveDeviceTimezoneByGeolocation,
                                true);
}

// static
void Preferences::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  std::string hardware_keyboard_id;
  // TODO(yusukes): Remove the runtime hack.
  if (base::SysInfo::IsRunningOnChromeOS()) {
    DCHECK(g_browser_process);
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    hardware_keyboard_id =
        local_state->GetString(prefs::kHardwareKeyboardLayout);
  } else {
    hardware_keyboard_id = "xkb:us::eng";  // only for testing.
  }

  registry->RegisterBooleanPref(
      prefs::kPerformanceTracingEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kTapToClickEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kTapDraggingEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kEnableTouchpadThreeFingerClick,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kNaturalScroll, base::CommandLine::ForCurrentProcess()->HasSwitch(
                                 switches::kNaturalScrollDefault),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kPrimaryMouseButtonRight,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kLabsMediaplayerEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLabsAdvancedFilesystemEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityStickyKeysEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityLargeCursorEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilitySpokenFeedbackEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityHighContrastEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityScreenMagnifierEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityScreenMagnifierType,
      ui::kDefaultMagnifierType,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(
      prefs::kAccessibilityScreenMagnifierScale,
      std::numeric_limits<double>::min(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityAutoclickEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kAccessibilityAutoclickDelayMs,
      ash::AutoclickController::kDefaultAutoclickDelayMs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kAccessibilityVirtualKeyboardEnabled,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShouldAlwaysShowAccessibilityMenu,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kMouseSensitivity,
      3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kTouchpadSensitivity,
      3,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterBooleanPref(
      prefs::kUse24HourClock,
      base::GetHourClockType() == base::k24HourClock,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDisableDrive,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDisableDriveOverCellular,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDisableDriveHostedFiles,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  // We don't sync prefs::kLanguageCurrentInputMethod and PreviousInputMethod
  // because they're just used to track the logout state of the device.
  registry->RegisterStringPref(
      prefs::kLanguageCurrentInputMethod,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kLanguagePreviousInputMethod,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kLanguagePreferredLanguages,
      kFallbackInputMethodLocale,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kLanguagePreloadEngines,
      hardware_keyboard_id,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kLanguageEnabledExtensionImes,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterIntegerPref(
      prefs::kLanguageRemapSearchKeyTo,
      input_method::kSearchKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapControlKeyTo,
      input_method::kControlKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapAltKeyTo,
      input_method::kAltKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // We don't sync the CapsLock remapping pref, since the UI hides this pref
  // on certain devices, so syncing a non-default value to a device that
  // doesn't allow changing the pref would be odd. http://crbug.com/167237
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapCapsLockKeyTo,
      input_method::kCapsLockKey,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageRemapDiamondKeyTo,
      input_method::kControlKey,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PRIORITY_PREF);
  // The following pref isn't synced since the user may desire a different value
  // depending on whether an external keyboard is attached to a particular
  // device.
  registry->RegisterBooleanPref(
      prefs::kLanguageSendFunctionKeys,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLanguageXkbAutoRepeatEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageXkbAutoRepeatDelay,
      language_prefs::kXkbAutoRepeatDelayInMs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLanguageXkbAutoRepeatInterval,
      language_prefs::kXkbAutoRepeatIntervalInMs,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // We don't sync wake-on-wifi related prefs because they are device specific.
  registry->RegisterBooleanPref(
      prefs::kWakeOnWifiSsid,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  // Mobile plan notifications default to on.
  registry->RegisterBooleanPref(
      prefs::kShowPlanNotifications,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  // 3G first-time usage promo will be shown at least once.
  registry->RegisterBooleanPref(
      prefs::kShow3gPromoNotification,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  // Initially all existing users would see "What's new" for current version
  // after update.
  registry->RegisterStringPref(prefs::kChromeOSReleaseNotesVersion,
                               "0.0.0.0",
                               user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kExternalStorageDisabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterStringPref(
      prefs::kTermsOfServiceURL,
      "",
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kTouchHudProjectionEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kTouchVirtualKeyboardEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  input_method::InputMethodSyncer::RegisterProfilePrefs(registry);

  registry->RegisterBooleanPref(
      prefs::kResolveTimezoneByGeolocation, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kCaptivePortalAuthenticationIgnoresProxy, true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

void Preferences::InitUserPrefs(PrefServiceSyncable* prefs) {
  prefs_ = prefs;

  BooleanPrefMember::NamedChangeCallback callback =
      base::Bind(&Preferences::OnPreferenceChanged, base::Unretained(this));

  performance_tracing_enabled_.Init(prefs::kPerformanceTracingEnabled,
                                    prefs, callback);
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, callback);
  tap_dragging_enabled_.Init(prefs::kTapDraggingEnabled, prefs, callback);
  three_finger_click_enabled_.Init(prefs::kEnableTouchpadThreeFingerClick,
      prefs, callback);
  natural_scroll_.Init(prefs::kNaturalScroll, prefs, callback);
  mouse_sensitivity_.Init(prefs::kMouseSensitivity, prefs, callback);
  touchpad_sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, callback);
  primary_mouse_button_right_.Init(prefs::kPrimaryMouseButtonRight,
                                   prefs, callback);
  download_default_directory_.Init(prefs::kDownloadDefaultDirectory,
                                   prefs, callback);
  touch_hud_projection_enabled_.Init(prefs::kTouchHudProjectionEnabled,
                                     prefs, callback);
  preload_engines_.Init(prefs::kLanguagePreloadEngines, prefs, callback);
  enabled_extension_imes_.Init(prefs::kLanguageEnabledExtensionImes,
                               prefs, callback);
  current_input_method_.Init(prefs::kLanguageCurrentInputMethod,
                             prefs, callback);
  previous_input_method_.Init(prefs::kLanguagePreviousInputMethod,
                              prefs, callback);

  xkb_auto_repeat_enabled_.Init(
      prefs::kLanguageXkbAutoRepeatEnabled, prefs, callback);
  xkb_auto_repeat_delay_pref_.Init(
      prefs::kLanguageXkbAutoRepeatDelay, prefs, callback);
  xkb_auto_repeat_interval_pref_.Init(
      prefs::kLanguageXkbAutoRepeatInterval, prefs, callback);

  wake_on_wifi_ssid_.Init(prefs::kWakeOnWifiSsid, prefs, callback);

  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(prefs::kResolveTimezoneByGeolocation, callback);
}

void Preferences::Init(Profile* profile, const user_manager::User* user) {
  DCHECK(profile);
  DCHECK(user);
  PrefServiceSyncable* prefs = PrefServiceSyncable::FromProfile(profile);
  // This causes OnIsSyncingChanged to be called when the value of
  // PrefService::IsSyncing() changes.
  prefs->AddObserver(this);
  user_ = user;
  user_is_primary_ =
      user_manager::UserManager::Get()->GetPrimaryUser() == user_;
  InitUserPrefs(prefs);

  user_manager::UserManager::Get()->AddSessionStateObserver(this);

  UserSessionManager* session_manager = UserSessionManager::GetInstance();
  DCHECK(session_manager);
  ime_state_ = session_manager->GetDefaultIMEState(profile);

  // Initialize preferences to currently saved state.
  ApplyPreferences(REASON_INITIALIZATION, "");

  // Note that |ime_state_| was modified by ApplyPreferences(), and
  // SetState() is modifying |current_input_method_| (via
  // PersistUserInputMethod() ). This way SetState() here may be called only
  // after ApplyPreferences().
  input_method_manager_->SetState(ime_state_);

  input_method_syncer_.reset(
      new input_method::InputMethodSyncer(prefs, ime_state_));
  input_method_syncer_->Initialize();

  // If a guest is logged in, initialize the prefs as if this is the first
  // login. For a regular user this is done in
  // UserSessionManager::InitProfilePreferences().
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kGuestSession))
    session_manager->SetFirstLoginPrefs(profile, std::string(), std::string());
}

void Preferences::InitUserPrefsForTesting(
    PrefServiceSyncable* prefs,
    const user_manager::User* user,
    scoped_refptr<input_method::InputMethodManager::State> ime_state) {
  user_ = user;
  ime_state_ = ime_state;

  if (ime_state.get())
    input_method_manager_->SetState(ime_state);

  InitUserPrefs(prefs);

  input_method_syncer_.reset(
      new input_method::InputMethodSyncer(prefs, ime_state_));
  input_method_syncer_->Initialize();
}

void Preferences::SetInputMethodListForTesting() {
  SetInputMethodList();
}

void Preferences::OnPreferenceChanged(const std::string& pref_name) {
  ApplyPreferences(REASON_PREF_CHANGED, pref_name);
}

void Preferences::ApplyPreferences(ApplyReason reason,
                                   const std::string& pref_name) {
  DCHECK(reason != REASON_PREF_CHANGED || !pref_name.empty());
  const bool user_is_owner =
      user_manager::UserManager::Get()->GetOwnerEmail() == user_->email();
  const bool user_is_active = user_->is_active();

  system::TouchpadSettings touchpad_settings;
  system::MouseSettings mouse_settings;

  if (user_is_primary_ && (reason == REASON_INITIALIZATION ||
                           pref_name == prefs::kPerformanceTracingEnabled)) {
    const bool enabled = performance_tracing_enabled_.GetValue();
    if (enabled)
      tracing_manager_ = TracingManager::Create();
    else
      tracing_manager_.reset();
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kTapToClickEnabled) {
    const bool enabled = tap_to_click_enabled_.GetValue();
    if (user_is_active)
      touchpad_settings.SetTapToClick(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapToClick.Started", enabled);

    // Save owner preference in local state to use on login screen.
    if (user_is_owner) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerTapToClickEnabled) != enabled)
        prefs->SetBoolean(prefs::kOwnerTapToClickEnabled, enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTapDraggingEnabled) {
    const bool enabled = tap_dragging_enabled_.GetValue();
    if (user_is_active)
      touchpad_settings.SetTapDragging(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.TapDragging.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kEnableTouchpadThreeFingerClick) {
    const bool enabled = three_finger_click_enabled_.GetValue();
    if (user_is_active)
      touchpad_settings.SetThreeFingerClick(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerClick.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.ThreeFingerClick.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kNaturalScroll) {
    // Force natural scroll default if we've sync'd and if the cmd line arg is
    // set.
    ForceNaturalScrollDefault();

    const bool enabled = natural_scroll_.GetValue();
    DVLOG(1) << "Natural scroll set to " << enabled;
    if (user_is_active)
      touchpad_settings.SetNaturalScroll(enabled);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Changed", enabled);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Started", enabled);
  }
  if (reason != REASON_PREF_CHANGED || pref_name == prefs::kMouseSensitivity) {
    const int sensitivity = mouse_sensitivity_.GetValue();
    if (user_is_active)
      mouse_settings.SetSensitivity(sensitivity);
    if (reason == REASON_PREF_CHANGED) {
      UMA_HISTOGRAM_ENUMERATION("Mouse.PointerSensitivity.Changed",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    } else if (reason == REASON_INITIALIZATION) {
      UMA_HISTOGRAM_ENUMERATION("Mouse.PointerSensitivity.Started",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchpadSensitivity) {
    const int sensitivity = touchpad_sensitivity_.GetValue();
    if (user_is_active)
      touchpad_settings.SetSensitivity(sensitivity);
    if (reason == REASON_PREF_CHANGED) {
      UMA_HISTOGRAM_ENUMERATION("Touchpad.PointerSensitivity.Changed",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    } else if (reason == REASON_INITIALIZATION) {
      UMA_HISTOGRAM_ENUMERATION("Touchpad.PointerSensitivity.Started",
                                sensitivity,
                                system::kMaxPointerSensitivity + 1);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kPrimaryMouseButtonRight) {
    const bool right = primary_mouse_button_right_.GetValue();
    if (user_is_active)
      mouse_settings.SetPrimaryButtonRight(right);
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN("Mouse.PrimaryButtonRight.Changed", right);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN("Mouse.PrimaryButtonRight.Started", right);
    // Save owner preference in local state to use on login screen.
    if (user_is_owner) {
      PrefService* prefs = g_browser_process->local_state();
      if (prefs->GetBoolean(prefs::kOwnerPrimaryMouseButtonRight) != right)
        prefs->SetBoolean(prefs::kOwnerPrimaryMouseButtonRight, right);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kDownloadDefaultDirectory) {
    const bool default_download_to_drive = drive::util::IsUnderDriveMountPoint(
        download_default_directory_.GetValue());
    if (reason == REASON_PREF_CHANGED)
      UMA_HISTOGRAM_BOOLEAN(
          "FileBrowser.DownloadDestination.IsGoogleDrive.Changed",
          default_download_to_drive);
    else if (reason == REASON_INITIALIZATION)
      UMA_HISTOGRAM_BOOLEAN(
          "FileBrowser.DownloadDestination.IsGoogleDrive.Started",
          default_download_to_drive);
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kTouchHudProjectionEnabled) {
    if (user_is_active) {
      const bool enabled = touch_hud_projection_enabled_.GetValue();
      // There may not be a shell, e.g., in some unit tests.
      if (ash::Shell::HasInstance())
        ash::Shell::GetInstance()->SetTouchHudProjectionEnabled(enabled);
    }
  }

  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kLanguageXkbAutoRepeatEnabled) {
    if (user_is_active) {
      const bool enabled = xkb_auto_repeat_enabled_.GetValue();
      input_method::InputMethodManager::Get()
          ->GetImeKeyboard()
          ->SetAutoRepeatEnabled(enabled);
    }
  }
  if (reason != REASON_PREF_CHANGED ||
      pref_name == prefs::kLanguageXkbAutoRepeatDelay ||
      pref_name == prefs::kLanguageXkbAutoRepeatInterval) {
    if (user_is_active)
      UpdateAutoRepeatRate();
  }

  if (reason == REASON_INITIALIZATION)
    SetInputMethodList();

  if (pref_name == prefs::kLanguagePreloadEngines &&
      reason == REASON_PREF_CHANGED) {
    SetLanguageConfigStringListAsCSV(language_prefs::kGeneralSectionName,
                                     language_prefs::kPreloadEnginesConfigName,
                                     preload_engines_.GetValue());
  }

  if ((reason == REASON_INITIALIZATION) ||
      (pref_name == prefs::kLanguageEnabledExtensionImes &&
       reason == REASON_PREF_CHANGED)) {
    std::string value(enabled_extension_imes_.GetValue());

    std::vector<std::string> split_values;
    if (!value.empty())
      base::SplitString(value, ',', &split_values);

    ime_state_->SetEnabledExtensionImes(&split_values);
  }

  if (user_is_active) {
    system::InputDeviceSettings::Get()->UpdateTouchpadSettings(
        touchpad_settings);
    system::InputDeviceSettings::Get()->UpdateMouseSettings(mouse_settings);
  }

  if (user_is_primary_ && (reason != REASON_PREF_CHANGED ||
                           pref_name == prefs::kWakeOnWifiSsid)) {
    int features = wake_on_wifi_ssid_.GetValue() ?
        WakeOnWifiManager::WAKE_ON_SSID : WakeOnWifiManager::WAKE_ON_NONE;
    // The flag enables wake on packets but doesn't update a preference.
    if (base::CommandLine::ForCurrentProcess()->
            HasSwitch(switches::kWakeOnPackets)) {
      features |= WakeOnWifiManager::WAKE_ON_PACKET;
    }
    WakeOnWifiManager::Get()->OnPreferenceChanged(
        static_cast<WakeOnWifiManager::WakeOnWifiFeature>(features));
  }

  if (pref_name == prefs::kResolveTimezoneByGeolocation &&
      reason != REASON_ACTIVE_USER_CHANGED) {
    const bool value = prefs_->GetBoolean(prefs::kResolveTimezoneByGeolocation);
    if (user_is_owner) {
      g_browser_process->local_state()->SetBoolean(
          prefs::kResolveDeviceTimezoneByGeolocation, value);
    }
    if (user_is_primary_) {
      if (value) {
        g_browser_process->platform_part()->GetTimezoneResolver()->Start();
      } else {
        g_browser_process->platform_part()->GetTimezoneResolver()->Stop();
        if (reason == REASON_PREF_CHANGED) {
          // Allow immediate timezone update on Stop + Start.
          g_browser_process->local_state()->ClearPref(
              TimeZoneResolver::kLastTimeZoneRefreshTime);
        }
      }
    }
  }
}

void Preferences::OnIsSyncingChanged() {
  DVLOG(1) << "OnIsSyncingChanged";
  ForceNaturalScrollDefault();
}

void Preferences::ForceNaturalScrollDefault() {
  DVLOG(1) << "ForceNaturalScrollDefault";
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNaturalScrollDefault) &&
      prefs_->IsSyncing() && !prefs_->GetUserPrefValue(prefs::kNaturalScroll)) {
    DVLOG(1) << "Natural scroll forced to true";
    natural_scroll_.SetValue(true);
    UMA_HISTOGRAM_BOOLEAN("Touchpad.NaturalScroll.Forced", true);
  }
}

void Preferences::SetLanguageConfigStringListAsCSV(const char* section,
                                                   const char* name,
                                                   const std::string& value) {
  VLOG(1) << "Setting " << name << " to '" << value << "'";

  std::vector<std::string> split_values;
  if (!value.empty())
    base::SplitString(value, ',', &split_values);

  // Transfers the xkb id to extension-xkb id.
  if (input_method_manager_->MigrateInputMethods(&split_values))
    preload_engines_.SetValue(JoinString(split_values, ','));

  if (section == std::string(language_prefs::kGeneralSectionName) &&
      name == std::string(language_prefs::kPreloadEnginesConfigName)) {
    ime_state_->ReplaceEnabledInputMethods(split_values);
    return;
  }
}

void Preferences::SetInputMethodList() {
  // When |preload_engines_| are set, InputMethodManager::ChangeInputMethod()
  // might be called to change the current input method to the first one in the
  // |preload_engines_| list. This also updates previous/current input method
  // prefs. That's why GetValue() calls are placed before the
  // SetLanguageConfigStringListAsCSV() call below.
  const std::string previous_input_method_id =
      previous_input_method_.GetValue();
  const std::string current_input_method_id = current_input_method_.GetValue();
  SetLanguageConfigStringListAsCSV(language_prefs::kGeneralSectionName,
                                   language_prefs::kPreloadEnginesConfigName,
                                   preload_engines_.GetValue());

  // ChangeInputMethod() has to be called AFTER the value of |preload_engines_|
  // is sent to the InputMethodManager. Otherwise, the ChangeInputMethod request
  // might be ignored as an invalid input method ID. The ChangeInputMethod()
  // calls are also necessary to restore the previous/current input method prefs
  // which could have been modified by the SetLanguageConfigStringListAsCSV call
  // above to the original state.
  if (!previous_input_method_id.empty())
    ime_state_->ChangeInputMethod(previous_input_method_id,
                                  false /* show_message */);
  if (!current_input_method_id.empty())
    ime_state_->ChangeInputMethod(current_input_method_id,
                                  false /* show_message */);
}

void Preferences::UpdateAutoRepeatRate() {
  input_method::AutoRepeatRate rate;
  rate.initial_delay_in_ms = xkb_auto_repeat_delay_pref_.GetValue();
  rate.repeat_interval_in_ms = xkb_auto_repeat_interval_pref_.GetValue();
  DCHECK(rate.initial_delay_in_ms > 0);
  DCHECK(rate.repeat_interval_in_ms > 0);
  input_method::InputMethodManager::Get()
      ->GetImeKeyboard()
      ->SetAutoRepeatRate(rate);
}

void Preferences::OnTouchHudProjectionToggled(bool enabled) {
  if (touch_hud_projection_enabled_.GetValue() == enabled)
    return;
  if (!user_->is_active())
    return;
  touch_hud_projection_enabled_.SetValue(enabled);
}

void Preferences::ActiveUserChanged(const user_manager::User* active_user) {
  if (active_user != user_)
    return;
  ApplyPreferences(REASON_ACTIVE_USER_CHANGED, "");
}

}  // namespace chromeos
