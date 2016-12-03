// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/permissions/PermissionController.h"

#include "core/frame/LocalFrame.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/platform/modules/permissions/WebPermissionClient.h"

namespace blink {

PermissionController::~PermissionController()
{
}

void PermissionController::provideTo(LocalFrame& frame, WebPermissionClient* client)
{
    ASSERT(RuntimeEnabledFeatures::permissionsEnabled());

    PermissionController* controller = new PermissionController(frame, client);
    Supplement<LocalFrame>::provideTo(frame, supplementName(), controller);
}

PermissionController* PermissionController::from(LocalFrame& frame)
{
    return static_cast<PermissionController*>(Supplement<LocalFrame>::from(frame, supplementName()));
}

PermissionController::PermissionController(LocalFrame& frame, WebPermissionClient* client)
    : DOMWindowProperty(&frame)
    , m_client(client)
{
}

const char* PermissionController::supplementName()
{
    return "PermissionController";
}

WebPermissionClient* PermissionController::client() const
{
    return m_client;
}

void PermissionController::willDestroyGlobalObjectInFrame()
{
    m_client = nullptr;
}

DEFINE_TRACE(PermissionController)
{
    DOMWindowProperty::trace(visitor);
    Supplement<LocalFrame>::trace(visitor);
}

} // namespace blink
