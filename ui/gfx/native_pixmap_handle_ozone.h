// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_PIXMAP_HANDLE_OZONE_H_
#define UI_GFX_NATIVE_PIXMAP_HANDLE_OZONE_H_

#include <stdint.h>
#include <vector>

#include "base/file_descriptor_posix.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// NativePixmapPlane is used to carry the plane related information for GBM
// buffer. More fields can be added if they are plane specific.
struct GFX_EXPORT NativePixmapPlane {
  NativePixmapPlane();
  NativePixmapPlane(int stride, int offset, uint64_t modifier);
  NativePixmapPlane(const NativePixmapPlane& other);
  ~NativePixmapPlane();

  // The strides and offsets in bytes to be used when accessing the buffers via
  // a memory mapping. One per plane per entry.
  int stride;
  int offset;
  // The modifier is retrieved from GBM library and passed to EGL driver.
  // Generally it's platform specific, and we don't need to modify it in
  // Chromium code. Also one per plane per entry.
  uint64_t modifier;
};

struct GFX_EXPORT NativePixmapHandle {
  NativePixmapHandle();
  NativePixmapHandle(const NativePixmapHandle& other);

  ~NativePixmapHandle();
  // File descriptors for the underlying memory objects (usually dmabufs).
  std::vector<base::FileDescriptor> fds;

  std::vector<NativePixmapPlane> planes;
};

}  // namespace gfx

#endif  // UI_GFX_NATIVE_PIXMAP_HANDLE_OZONE_H_
