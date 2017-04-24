// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_GPU_UTILS_H_

#include "content/common/content_export.h"
#include "gpu/command_buffer/service/gpu_preferences.h"

namespace content {

CONTENT_EXPORT const gpu::GpuPreferences GetGpuPreferencesFromCommandLine();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_UTILS_H_
