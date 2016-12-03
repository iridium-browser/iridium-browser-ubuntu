// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_METRICS_H_
#define CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_METRICS_H_

#include <string>

#include "gpu/command_buffer/common/constants.h"

namespace content {
namespace command_buffer_metrics {

enum ContextType {
  DISPLAY_COMPOSITOR_ONSCREEN_CONTEXT,
  BROWSER_OFFSCREEN_MAINTHREAD_CONTEXT,
  BROWSER_WORKER_CONTEXT,
  RENDER_COMPOSITOR_CONTEXT,
  RENDER_WORKER_CONTEXT,
  RENDERER_MAINTHREAD_CONTEXT,
  GPU_VIDEO_ACCELERATOR_CONTEXT,
  OFFSCREEN_VIDEO_CAPTURE_CONTEXT,
  OFFSCREEN_CONTEXT_FOR_WEBGL,
  CONTEXT_TYPE_UNKNOWN,
  MEDIA_CONTEXT,
  BLIMP_RENDER_COMPOSITOR_CONTEXT,
  BLIMP_RENDER_WORKER_CONTEXT,
  OFFSCREEN_CONTEXT_FOR_TESTING = CONTEXT_TYPE_UNKNOWN,
};

std::string ContextTypeToString(ContextType type);

void UmaRecordContextInitFailed(ContextType type);

void UmaRecordContextLost(ContextType type,
                          gpu::error::Error error,
                          gpu::error::ContextLostReason reason);

}  // namespace command_buffer_metrics
}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_COMMAND_BUFFER_METRICS_H_
