// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/cast_config_delegate_chromeos.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/cast_devices_private/cast_devices_private_api.h"
#include "chrome/browser/extensions/api/tab_capture/tab_capture_api.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/api/cast_devices_private.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"

namespace chromeos {
namespace {

Profile* GetProfile() {
  // TODO(jdufault): Figure out how to correctly handle multiprofile mode.
  // See crbug.com/488751
  return ProfileManager::GetActiveUserProfile();
}

// Returns the cast extension if it exists.
const extensions::Extension* FindCastExtension() {
  Profile* profile = GetProfile();
  const extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  const extensions::ExtensionSet& enabled_extensions =
      extension_registry->enabled_extensions();

  for (size_t i = 0; i < arraysize(extensions::kChromecastExtensionIds); ++i) {
    const std::string extension_id(extensions::kChromecastExtensionIds[i]);
    if (enabled_extensions.Contains(extension_id)) {
      return extension_registry->GetExtensionById(
          extension_id, extensions::ExtensionRegistry::ENABLED);
    }
  }
  return nullptr;
}

}  // namespace

CastConfigDelegateChromeos::CastConfigDelegateChromeos() {
}

CastConfigDelegateChromeos::~CastConfigDelegateChromeos() {
}

bool CastConfigDelegateChromeos::HasCastExtension() const {
  return FindCastExtension() != nullptr;
}

CastConfigDelegateChromeos::DeviceUpdateSubscription
CastConfigDelegateChromeos::RegisterDeviceUpdateObserver(
    const ReceiversAndActivitesCallback& callback) {
  auto listeners = extensions::CastDeviceUpdateListeners::Get(GetProfile());
  return listeners->RegisterCallback(callback);
}

void CastConfigDelegateChromeos::RequestDeviceRefresh() {
  scoped_ptr<base::ListValue> args =
      extensions::api::cast_devices_private::UpdateDevicesRequested::Create();
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::CAST_DEVICES_PRIVATE_ON_UPDATE_DEVICES_REQUESTED,
      extensions::api::cast_devices_private::UpdateDevicesRequested::kEventName,
      args.Pass()));
  extensions::EventRouter::Get(GetProfile())
      ->DispatchEventToExtension(FindCastExtension()->id(), event.Pass());
}

void CastConfigDelegateChromeos::CastToReceiver(
    const std::string& receiver_id) {
  scoped_ptr<base::ListValue> args =
      extensions::api::cast_devices_private::StartCast::Create(receiver_id);
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::CAST_DEVICES_PRIVATE_ON_START_CAST,
      extensions::api::cast_devices_private::StartCast::kEventName,
      args.Pass()));
  extensions::EventRouter::Get(GetProfile())
      ->DispatchEventToExtension(FindCastExtension()->id(), event.Pass());
}

void CastConfigDelegateChromeos::StopCasting() {
  scoped_ptr<base::ListValue> args =
      extensions::api::cast_devices_private::StopCast::Create("user-stop");
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::events::CAST_DEVICES_PRIVATE_ON_STOP_CAST,
      extensions::api::cast_devices_private::StopCast::kEventName,
      args.Pass()));
  extensions::EventRouter::Get(GetProfile())
      ->DispatchEventToExtension(FindCastExtension()->id(), event.Pass());
}

void CastConfigDelegateChromeos::LaunchCastOptions() {
  chrome::NavigateParams params(
      ProfileManager::GetActiveUserProfile(),
      FindCastExtension()->GetResourceURL("options.html"),
      ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  chrome::Navigate(&params);
}

}  // namespace chromeos
