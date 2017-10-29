// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/media_devices_selection_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace {

const char kAudio[] = "mic";
const char kVideo[] = "camera";

}  // namespace

namespace options {

MediaDevicesSelectionHandler::MediaDevicesSelectionHandler() {}

MediaDevicesSelectionHandler::~MediaDevicesSelectionHandler() {
  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
}

void MediaDevicesSelectionHandler::GetLocalizedValues(
    base::DictionaryValue* values) {
  DCHECK(values);

  static OptionsStringResource resources[] = {
    { "mediaSelectMicLabel", IDS_MEDIA_SELECTED_MIC_LABEL },
    { "mediaSelectCameraLabel", IDS_MEDIA_SELECTED_CAMERA_LABEL },
  };

  RegisterStrings(values, resources, arraysize(resources));
}

void MediaDevicesSelectionHandler::InitializePage() {
  // Register to the device observer list to get up-to-date device lists.
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);

  // Update the device selection menus.
  UpdateDevicesMenuForType(AUDIO);
  UpdateDevicesMenuForType(VIDEO);
}

void MediaDevicesSelectionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("setDefaultCaptureDevice",
      base::Bind(&MediaDevicesSelectionHandler::SetDefaultCaptureDevice,
                 base::Unretained(this)));
}

void MediaDevicesSelectionHandler::OnUpdateAudioDevices(
    const content::MediaStreamDevices& devices) {
  UpdateDevicesMenu(AUDIO, devices);
}

void MediaDevicesSelectionHandler::OnUpdateVideoDevices(
    const content::MediaStreamDevices& devices) {
  UpdateDevicesMenu(VIDEO, devices);
}

void MediaDevicesSelectionHandler::SetDefaultCaptureDevice(
    const base::ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());
  std::string type, device;
  if (!(args->GetString(0, &type) && args->GetString(1, &device))) {
    NOTREACHED();
    return;
  }

  DCHECK(!type.empty());
  DCHECK(!device.empty());

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  if (type == kAudio)
    prefs->SetString(prefs::kDefaultAudioCaptureDevice, device);
  else if (type == kVideo)
    prefs->SetString(prefs::kDefaultVideoCaptureDevice, device);
  else
    NOTREACHED();
}

void MediaDevicesSelectionHandler::UpdateDevicesMenu(
    DeviceType type, const content::MediaStreamDevices& devices) {
  // Get the default device unique id from prefs.
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  std::string default_device;
  std::string device_type;
  switch (type) {
    case AUDIO:
      default_device = prefs->GetString(prefs::kDefaultAudioCaptureDevice);
      device_type = kAudio;
      break;
    case VIDEO:
      default_device = prefs->GetString(prefs::kDefaultVideoCaptureDevice);
      device_type = kVideo;
      break;
  }

  // Build the list of devices to send to JS.
  std::string default_id;
  base::ListValue device_list;
  for (size_t i = 0; i < devices.size(); ++i) {
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
    entry->SetString("name", GetDeviceDisplayName(devices[i]));
    entry->SetString("id",  devices[i].id);
    device_list.Append(std::move(entry));
    if (devices[i].id == default_device)
      default_id = default_device;
  }

  // Use the first device as the default device if the preferred default device
  // does not exist in the OS.
  if (!devices.empty() && default_id.empty())
    default_id = devices[0].id;

  base::Value default_value(default_id);
  base::Value type_value(device_type);
  web_ui()->CallJavascriptFunctionUnsafe("ContentSettings.updateDevicesMenu",
                                         type_value, device_list,
                                         default_value);
}

std::string MediaDevicesSelectionHandler::GetDeviceDisplayName(
    const content::MediaStreamDevice& device) const {
  std::string facing_info;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  switch (device.video_facing) {
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_USER:
      facing_info = l10n_util::GetStringUTF8(IDS_CAMERA_FACING_USER);
      break;
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT:
      facing_info = l10n_util::GetStringUTF8(IDS_CAMERA_FACING_ENVIRONMENT);
      break;
    case media::VideoFacingMode::MEDIA_VIDEO_FACING_NONE:
      break;
    case media::VideoFacingMode::NUM_MEDIA_VIDEO_FACING_MODES:
      NOTREACHED();
      break;
  }
#endif

  if (facing_info.empty())
    return device.name;
  return device.name + " " + facing_info;
}

void MediaDevicesSelectionHandler::UpdateDevicesMenuForType(DeviceType type) {
  content::MediaStreamDevices devices;
  switch (type) {
    case AUDIO:
      devices = MediaCaptureDevicesDispatcher::GetInstance()->
          GetAudioCaptureDevices();
      break;
    case VIDEO:
      devices = MediaCaptureDevicesDispatcher::GetInstance()->
          GetVideoCaptureDevices();
      break;
  }

  UpdateDevicesMenu(type, devices);
}

}  // namespace options
