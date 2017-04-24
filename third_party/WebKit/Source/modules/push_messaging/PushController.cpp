// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/push_messaging/PushController.h"

#include "public/platform/modules/push_messaging/WebPushClient.h"
#include "wtf/Assertions.h"

namespace blink {

PushController::PushController(LocalFrame& frame, WebPushClient* client)
    : Supplement<LocalFrame>(frame), m_client(client) {}

WebPushClient& PushController::clientFrom(LocalFrame* frame) {
  PushController* controller = PushController::from(frame);
  DCHECK(controller);
  WebPushClient* client = controller->client();
  DCHECK(client);
  return *client;
}

const char* PushController::supplementName() {
  return "PushController";
}

void providePushControllerTo(LocalFrame& frame, WebPushClient* client) {
  PushController::provideTo(frame, PushController::supplementName(),
                            new PushController(frame, client));
}

}  // namespace blink
