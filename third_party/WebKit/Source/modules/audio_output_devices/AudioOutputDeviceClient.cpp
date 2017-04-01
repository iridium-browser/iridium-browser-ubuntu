// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/audio_output_devices/AudioOutputDeviceClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"

namespace blink {

AudioOutputDeviceClient::AudioOutputDeviceClient(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

const char* AudioOutputDeviceClient::supplementName() {
  return "AudioOutputDeviceClient";
}

AudioOutputDeviceClient* AudioOutputDeviceClient::from(
    ExecutionContext* context) {
  if (!context || !context->isDocument())
    return nullptr;

  const Document* document = toDocument(context);
  if (!document->frame())
    return nullptr;

  return static_cast<AudioOutputDeviceClient*>(
      Supplement<LocalFrame>::from(document->frame(), supplementName()));
}

void provideAudioOutputDeviceClientTo(LocalFrame& frame,
                                      AudioOutputDeviceClient* client) {
  Supplement<LocalFrame>::provideTo(
      frame, AudioOutputDeviceClient::supplementName(), client);
}

DEFINE_TRACE(AudioOutputDeviceClient) {
  Supplement<LocalFrame>::trace(visitor);
}

}  // namespace blink
