// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptedIdleTaskController_h
#define ScriptedIdleTaskController_h

#include "core/dom/ActiveDOMObject.h"
#include "core/dom/IdleCallbackDeadline.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class ExecutionContext;
class IdleRequestCallback;
class DocumentLoadTiming;

class ScriptedIdleTaskController : public RefCountedWillBeGarbageCollectedFinalized<ScriptedIdleTaskController>, public ActiveDOMObject {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(ScriptedIdleTaskController);
public:
    static PassRefPtrWillBeRawPtr<ScriptedIdleTaskController> create(ExecutionContext* context, const DocumentLoadTiming& timing)
    {
        return adoptRefWillBeNoop(new ScriptedIdleTaskController(context, timing));
    }
    ~ScriptedIdleTaskController();

    DECLARE_TRACE();

    using CallbackId = int;

    int registerCallback(IdleRequestCallback*, double timeoutMillis);
    void cancelCallback(CallbackId);

    // ActiveDOMObject interface.
    void stop() override;
    void suspend() override;
    void resume() override;
    bool hasPendingActivity() const override;

    void callbackFired(CallbackId, double deadlineSeconds, IdleCallbackDeadline::CallbackType);

private:
    ScriptedIdleTaskController(ExecutionContext*, const DocumentLoadTiming&);

    void runCallback(CallbackId, double deadlineMillis, IdleCallbackDeadline::CallbackType);

    const DocumentLoadTiming& m_timing;
    WebScheduler* m_scheduler; // Not owned.
    PersistentHeapHashMapWillBeHeapHashMap<CallbackId, Member<IdleRequestCallback>> m_callbacks;
    Vector<CallbackId> m_pendingTimeouts;
    CallbackId m_nextCallbackId;
    bool m_suspended;
};

} // namespace blink

#endif // ScriptedIdleTaskController_h
