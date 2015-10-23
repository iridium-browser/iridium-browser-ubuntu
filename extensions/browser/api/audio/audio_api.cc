// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/audio/audio_api.h"

#include "base/lazy_instance.h"
#include "base/values.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/audio.h"

namespace extensions {

namespace audio = api::audio;

static base::LazyInstance<BrowserContextKeyedAPIFactory<AudioAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<AudioAPI>* AudioAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

AudioAPI::AudioAPI(content::BrowserContext* context)
    : browser_context_(context), service_(AudioService::CreateInstance()) {
  service_->AddObserver(this);
}

AudioAPI::~AudioAPI() {
  service_->RemoveObserver(this);
  delete service_;
  service_ = NULL;
}

AudioService* AudioAPI::GetService() const {
  return service_;
}

void AudioAPI::OnDeviceChanged() {
  if (EventRouter::Get(browser_context_)) {
    scoped_ptr<Event> event(new Event(
        events::AUDIO_ON_DEVICE_CHANGED, audio::OnDeviceChanged::kEventName,
        scoped_ptr<base::ListValue>(new base::ListValue())));
    EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
  }
}

void AudioAPI::OnLevelChanged(const std::string& id, int level) {
  if (EventRouter::Get(browser_context_)) {
    scoped_ptr<base::ListValue> args = audio::OnLevelChanged::Create(id, level);
    scoped_ptr<Event> event(new Event(events::AUDIO_ON_LEVEL_CHANGED,
                                      audio::OnLevelChanged::kEventName,
                                      args.Pass()));
    EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
  }
}

void AudioAPI::OnMuteChanged(bool is_input, bool is_muted) {
  if (EventRouter::Get(browser_context_)) {
    scoped_ptr<base::ListValue> args =
        audio::OnMuteChanged::Create(is_input, is_muted);
    scoped_ptr<Event> event(new Event(events::AUDIO_ON_MUTE_CHANGED,
                                      audio::OnMuteChanged::kEventName,
                                      args.Pass()));
    EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
  }
}

void AudioAPI::OnDevicesChanged(const DeviceInfoList& devices) {
  if (EventRouter::Get(browser_context_)) {
    scoped_ptr<base::ListValue> args = audio::OnDevicesChanged::Create(devices);
    scoped_ptr<Event> event(new Event(events::AUDIO_ON_DEVICES_CHANGED,
                                      audio::OnDevicesChanged::kEventName,
                                      args.Pass()));
    EventRouter::Get(browser_context_)->BroadcastEvent(event.Pass());
  }
}

///////////////////////////////////////////////////////////////////////////////

bool AudioGetInfoFunction::RunAsync() {
  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);
  service->StartGetInfo(base::Bind(&AudioGetInfoFunction::OnGetInfoCompleted,
                                   this));
  return true;
}

void AudioGetInfoFunction::OnGetInfoCompleted(const OutputInfo& output_info,
                                              const InputInfo& input_info,
                                              bool success) {
  if (success)
    results_ = audio::GetInfo::Results::Create(output_info, input_info);
  else
    SetError("Error occurred when querying audio device information.");
  SendResponse(success);
}

///////////////////////////////////////////////////////////////////////////////

bool AudioSetActiveDevicesFunction::RunSync() {
  scoped_ptr<audio::SetActiveDevices::Params> params(
      audio::SetActiveDevices::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  service->SetActiveDevices(params->ids);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

bool AudioSetPropertiesFunction::RunSync() {
  scoped_ptr<audio::SetProperties::Params> params(
      audio::SetProperties::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  AudioService* service =
      AudioAPI::GetFactoryInstance()->Get(browser_context())->GetService();
  DCHECK(service);

  int volume_value = params->properties.volume.get() ?
      *params->properties.volume : -1;

  int gain_value = params->properties.gain.get() ?
      *params->properties.gain : -1;

  if (!service->SetDeviceProperties(params->id,
                                    params->properties.is_muted,
                                    volume_value,
                                    gain_value))
    return false;
  else
    return true;
}

///////////////////////////////////////////////////////////////////////////////

}  // namespace extensions
