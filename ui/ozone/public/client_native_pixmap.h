// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_H_
#define UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_H_

#include "ui/ozone/ozone_export.h"

namespace ui {

// This represents a buffer that can be written to directly by regular CPU code,
// but can also be read by the GPU.
// NativePixmap is its counterpart in GPU process.
class OZONE_EXPORT ClientNativePixmap {
 public:
  virtual ~ClientNativePixmap() {}

  virtual void* Map() = 0;
  virtual void Unmap() = 0;
  virtual void GetStride(int* stride) const = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_CLIENT_NATIVE_PIXMAP_H_
