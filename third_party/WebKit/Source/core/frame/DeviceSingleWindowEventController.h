// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceSingleWindowEventController_h
#define DeviceSingleWindowEventController_h

#include "core/CoreExport.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/PlatformEventController.h"
#include "platform/heap/Handle.h"

namespace blink {

class Document;
class Event;

class CORE_EXPORT DeviceSingleWindowEventController
    : public GarbageCollectedFinalized<DeviceSingleWindowEventController>,
      public PlatformEventController,
      public LocalDOMWindow::EventListenerObserver {
 public:
  virtual ~DeviceSingleWindowEventController();

  // Inherited from DeviceEventControllerBase.
  void didUpdateData() override;
  DECLARE_VIRTUAL_TRACE();

  // Inherited from LocalDOMWindow::EventListenerObserver.
  void didAddEventListener(LocalDOMWindow*, const AtomicString&) override;
  void didRemoveEventListener(LocalDOMWindow*, const AtomicString&) override;
  void didRemoveAllEventListeners(LocalDOMWindow*) override;

 protected:
  explicit DeviceSingleWindowEventController(Document&);

  Document& document() const { return *m_document; }

  void dispatchDeviceEvent(Event*);

  virtual Event* lastEvent() const = 0;
  virtual const AtomicString& eventTypeName() const = 0;
  virtual bool isNullEvent(Event*) const = 0;

 private:
  bool m_needsCheckingNullEvents;
  Member<Document> m_document;
};

}  // namespace blink

#endif  // DeviceSingleWindowEventController_h
