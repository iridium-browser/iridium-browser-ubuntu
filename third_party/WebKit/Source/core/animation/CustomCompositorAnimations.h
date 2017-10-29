// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomCompositorAnimations_h
#define CustomCompositorAnimations_h

#include "core/animation/Animation.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class Element;
class CompositorMutation;

class CustomCompositorAnimations final {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(CustomCompositorAnimations);

 public:
  CustomCompositorAnimations() {}
  void ApplyUpdate(Element&, const CompositorMutation&);

  DEFINE_INLINE_TRACE() { visitor->Trace(animation_); }

 private:
  Member<Animation> animation_;
};

}  // namespace blink

#endif
