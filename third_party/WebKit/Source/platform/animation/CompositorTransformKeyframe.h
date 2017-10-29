// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorTransformKeyframe_h
#define CompositorTransformKeyframe_h

#include "cc/animation/keyframed_animation_curve.h"
#include "platform/PlatformExport.h"
#include "platform/animation/CompositorKeyframe.h"
#include "platform/animation/CompositorTransformOperations.h"
#include "platform/animation/TimingFunction.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PLATFORM_EXPORT CompositorTransformKeyframe : public CompositorKeyframe {
  WTF_MAKE_NONCOPYABLE(CompositorTransformKeyframe);

 public:
  CompositorTransformKeyframe(double time,
                              CompositorTransformOperations value,
                              const TimingFunction&);
  ~CompositorTransformKeyframe();

  std::unique_ptr<cc::TransformKeyframe> CloneToCC() const;

  // CompositorKeyframe implementation.
  double Time() const override;
  const cc::TimingFunction* CcTimingFunction() const override;

 private:
  std::unique_ptr<cc::TransformKeyframe> transform_keyframe_;
};

}  // namespace blink

#endif  // CompositorTransformKeyframe_h
