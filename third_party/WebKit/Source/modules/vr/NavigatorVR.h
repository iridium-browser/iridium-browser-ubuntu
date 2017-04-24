// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorVR_h
#define NavigatorVR_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Navigator.h"
#include "core/page/FocusChangedObserver.h"
#include "modules/ModulesExport.h"
#include "modules/vr/VRDisplay.h"
#include "modules/vr/VRDisplayEvent.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebVector.h"
#include "wtf/Noncopyable.h"

namespace blink {

class Document;
class Navigator;
class VRController;

class MODULES_EXPORT NavigatorVR final
    : public GarbageCollectedFinalized<NavigatorVR>,
      public Supplement<Navigator>,
      public LocalDOMWindow::EventListenerObserver,
      public FocusChangedObserver {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorVR);
  WTF_MAKE_NONCOPYABLE(NavigatorVR);

 public:
  static NavigatorVR* from(Document&);
  static NavigatorVR& from(Navigator&);
  virtual ~NavigatorVR();

  static ScriptPromise getVRDisplays(ScriptState*, Navigator&);
  ScriptPromise getVRDisplays(ScriptState*);

  VRController* controller();
  Document* document();
  bool isFocused() { return m_focused; }

  // Queues up event to be fired soon.
  void enqueueVREvent(VRDisplayEvent*);

  // Dispatches a user gesture event immediately.
  void dispatchVRGestureEvent(VRDisplayEvent*);

  // Inherited from FocusChangedObserver.
  void focusedFrameChanged() override;

  // Inherited from LocalDOMWindow::EventListenerObserver.
  void didAddEventListener(LocalDOMWindow*, const AtomicString&) override;
  void didRemoveEventListener(LocalDOMWindow*, const AtomicString&) override;
  void didRemoveAllEventListeners(LocalDOMWindow*) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  friend class VRDisplay;
  friend class VRGetDevicesCallback;

  explicit NavigatorVR(Navigator&);

  static const char* supplementName();

  void fireVRDisplayPresentChange(VRDisplay*);

  Member<VRController> m_controller;

  // Whether this page is listening for vrdisplayactivate event.
  bool m_listeningForActivate = false;
  bool m_focused = false;
};

}  // namespace blink

#endif  // NavigatorVR_h
