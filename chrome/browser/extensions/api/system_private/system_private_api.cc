// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_private/system_private_api.h"

#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/system_private.h"
#include "chrome/common/pref_names.h"
#include "google_apis/google_api_keys.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/update_engine_client.h"
#else
#include "chrome/browser/upgrade_detector.h"
#endif

namespace {

// Maps prefs::kIncognitoModeAvailability values (0 = enabled, ...)
// to strings exposed to extensions.
const char* const kIncognitoModeAvailabilityStrings[] = {
  "enabled",
  "disabled",
  "forced"
};

// Property keys.
const char kBrightnessKey[] = "brightness";
const char kDownloadProgressKey[] = "downloadProgress";
const char kIsVolumeMutedKey[] = "isVolumeMuted";
const char kStateKey[] = "state";
const char kUserInitiatedKey[] = "userInitiated";
const char kVolumeKey[] = "volume";

// System update states.
const char kNotAvailableState[] = "NotAvailable";
const char kNeedRestartState[] = "NeedRestart";

#if defined(OS_CHROMEOS)
const char kUpdatingState[] = "Updating";
#endif  // defined(OS_CHROMEOS)

// Dispatches an extension event with |argument|
void DispatchEvent(extensions::events::HistogramValue histogram_value,
                   const std::string& event_name,
                   base::Value* argument) {
  scoped_ptr<base::ListValue> list_args(new base::ListValue());
  if (argument) {
    list_args->Append(argument);
  }
  g_browser_process->extension_event_router_forwarder()
      ->BroadcastEventToRenderers(histogram_value, event_name, list_args.Pass(),
                                  GURL());
}

}  // namespace

namespace extensions {

namespace system_private = api::system_private;

bool SystemPrivateGetIncognitoModeAvailabilityFunction::RunSync() {
  PrefService* prefs = GetProfile()->GetPrefs();
  int value = prefs->GetInteger(prefs::kIncognitoModeAvailability);
  EXTENSION_FUNCTION_VALIDATE(
      value >= 0 &&
      value < static_cast<int>(arraysize(kIncognitoModeAvailabilityStrings)));
  SetResult(new base::StringValue(kIncognitoModeAvailabilityStrings[value]));
  return true;
}

bool SystemPrivateGetUpdateStatusFunction::RunSync() {
  std::string state;
  double download_progress = 0;
#if defined(OS_CHROMEOS)
  // With UpdateEngineClient, we can provide more detailed information about
  // system updates on ChromeOS.
  const chromeos::UpdateEngineClient::Status status =
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient()->
      GetLastStatus();
  // |download_progress| is set to 1 after download finishes
  // (i.e. verify, finalize and need-reboot phase) to indicate the progress
  // even though |status.download_progress| is 0 in these phases.
  switch (status.status) {
    case chromeos::UpdateEngineClient::UPDATE_STATUS_ERROR:
      state = kNotAvailableState;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_IDLE:
      state = kNotAvailableState;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      state = kNotAvailableState;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATE_AVAILABLE:
      state = kUpdatingState;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_DOWNLOADING:
      state = kUpdatingState;
      download_progress = status.download_progress;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_VERIFYING:
      state = kUpdatingState;
      download_progress = 1;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_FINALIZING:
      state = kUpdatingState;
      download_progress = 1;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      state = kNeedRestartState;
      download_progress = 1;
      break;
    case chromeos::UpdateEngineClient::UPDATE_STATUS_REPORTING_ERROR_EVENT:
    case chromeos::UpdateEngineClient::UPDATE_STATUS_ATTEMPTING_ROLLBACK:
      state = kNotAvailableState;
      break;
  }
#else
  if (UpgradeDetector::GetInstance()->notify_upgrade()) {
    state = kNeedRestartState;
    download_progress = 1;
  } else {
    state = kNotAvailableState;
  }
#endif
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString(kStateKey, state);
  dict->SetDouble(kDownloadProgressKey, download_progress);
  SetResult(dict);

  return true;
}

bool SystemPrivateGetApiKeyFunction::RunSync() {
  SetResult(new base::StringValue(google_apis::GetAPIKey()));
  return true;
}

void DispatchVolumeChangedEvent(double volume, bool is_volume_muted) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetDouble(kVolumeKey, volume);
  dict->SetBoolean(kIsVolumeMutedKey, is_volume_muted);
  DispatchEvent(extensions::events::SYSTEM_PRIVATE_ON_VOLUME_CHANGED,
                system_private::OnVolumeChanged::kEventName, dict);
}

void DispatchBrightnessChangedEvent(int brightness, bool user_initiated) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger(kBrightnessKey, brightness);
  dict->SetBoolean(kUserInitiatedKey, user_initiated);
  DispatchEvent(extensions::events::SYSTEM_PRIVATE_ON_BRIGHTNESS_CHANGED,
                system_private::OnBrightnessChanged::kEventName, dict);
}

void DispatchScreenUnlockedEvent() {
  DispatchEvent(extensions::events::SYSTEM_PRIVATE_ON_SCREEN_UNLOCKED,
                system_private::OnScreenUnlocked::kEventName, NULL);
}

void DispatchWokeUpEvent() {
  DispatchEvent(extensions::events::SYSTEM_PRIVATE_ON_WOKE_UP,
                system_private::OnWokeUp::kEventName, NULL);
}

}  // namespace extensions
