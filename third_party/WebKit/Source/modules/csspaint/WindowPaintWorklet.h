// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowPaintWorklet_h
#define WindowPaintWorklet_h

#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class LocalDOMWindow;
class PaintWorklet;
class Worklet;

class MODULES_EXPORT WindowPaintWorklet final
    : public GarbageCollected<WindowPaintWorklet>,
      public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(WindowPaintWorklet);

 public:
  static WindowPaintWorklet& From(LocalDOMWindow&);
  static Worklet* paintWorklet(LocalDOMWindow&);
  PaintWorklet* paintWorklet();

  DECLARE_TRACE();

 private:
  explicit WindowPaintWorklet(LocalDOMWindow&);
  static const char* SupplementName();

  Member<PaintWorklet> paint_worklet_;
};

}  // namespace blink

#endif  // WindowPaintWorklet_h
