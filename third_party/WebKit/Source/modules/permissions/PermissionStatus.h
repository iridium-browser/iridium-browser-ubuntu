// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PermissionStatus_h
#define PermissionStatus_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/permissions/permission.mojom-blink.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;

// Expose the status of a given WebPermissionType for the current
// ExecutionContext.
class PermissionStatus final
    : public EventTargetWithInlineData
    , public ActiveScriptWrappable
    , public ActiveDOMObject {
    USING_GARBAGE_COLLECTED_MIXIN(PermissionStatus);
    DEFINE_WRAPPERTYPEINFO();

    using MojoPermissionName = mojom::blink::PermissionName;
    using MojoPermissionStatus = mojom::blink::PermissionStatus;

public:
    static PermissionStatus* take(ScriptPromiseResolver*, MojoPermissionStatus, MojoPermissionName);

    static PermissionStatus* createAndListen(ExecutionContext*, MojoPermissionStatus, MojoPermissionName);
    ~PermissionStatus() override;

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    // ScriptWrappable implementation.
    bool hasPendingActivity() const final;

    // ActiveDOMObject implementation.
    void suspend() override;
    void resume() override;
    void stop() override;

    String state() const;
    void permissionChanged(mojom::blink::PermissionStatus);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

    DECLARE_VIRTUAL_TRACE();

private:
    PermissionStatus(ExecutionContext*, MojoPermissionStatus, MojoPermissionName);

    void startListening();
    void stopListening();

    MojoPermissionStatus m_status;
    MojoPermissionName m_name;
    mojom::blink::PermissionServicePtr m_service;
};

} // namespace blink

#endif // PermissionStatus_h
