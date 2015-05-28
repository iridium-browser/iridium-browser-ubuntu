// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorServiceWorker_h
#define NavigatorServiceWorker_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Navigator;
class ServiceWorkerContainer;

class NavigatorServiceWorker final : public NoBaseWillBeGarbageCollected<NavigatorServiceWorker>, public WillBeHeapSupplement<Navigator>, DOMWindowProperty {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorServiceWorker);
    DECLARE_EMPTY_VIRTUAL_DESTRUCTOR_WILL_BE_REMOVED(NavigatorServiceWorker);
public:
    static NavigatorServiceWorker* from(Document&);
    static NavigatorServiceWorker& from(Navigator&);
    static NavigatorServiceWorker* toNavigatorServiceWorker(Navigator&);
    static const char* supplementName();

    static ServiceWorkerContainer* serviceWorker(Navigator&);

    DECLARE_VIRTUAL_TRACE();

private:
    explicit NavigatorServiceWorker(Navigator&);
    ServiceWorkerContainer* serviceWorker();

    // DOMWindowProperty override.
    virtual void willDetachGlobalObjectFromFrame() override;

    PersistentWillBeMember<ServiceWorkerContainer> m_serviceWorker;
};

} // namespace blink

#endif // NavigatorServiceWorker_h
