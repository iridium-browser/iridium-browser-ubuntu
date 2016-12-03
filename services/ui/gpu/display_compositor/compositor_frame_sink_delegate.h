// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_DELEGATE_H_
#define SERVICES_UI_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_DELEGATE_H_

#include "cc/surfaces/surface_id.h"

namespace ui {
namespace gpu {

class CompositorFrameSinkImpl;

// A CompositorFrameSinkDelegate decouples CompositorFrameSinks from their
// factories enabling them to be unit tested.
class CompositorFrameSinkDelegate {
 public:
  virtual void CompositorFrameSinkConnectionLost(int sink_id) = 0;

  virtual cc::SurfaceId GenerateSurfaceId() = 0;
};

}  // namespace gpu
}  // namespace ui

#endif  // SERVICES_UI_GPU_DISPLAY_COMPOSITOR_COMPOSITOR_FRAME_SINK_DELEGATE_H_
