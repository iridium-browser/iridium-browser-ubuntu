// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_SKIA_COMMON_H_
#define CC_TEST_SKIA_COMMON_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkFlattenable.h"
#include "third_party/skia/include/core/SkPixelRef.h"

namespace gfx {
class Rect;
class Size;
}

namespace cc {
class Picture;
class DisplayItemList;

void DrawPicture(unsigned char* buffer,
                 const gfx::Rect& layer_rect,
                 scoped_refptr<Picture> picture);

void DrawDisplayList(unsigned char* buffer,
                     const gfx::Rect& layer_rect,
                     scoped_refptr<DisplayItemList> list);

void CreateDiscardableBitmap(const gfx::Size& size, SkBitmap* bitmap);

}  // namespace cc

#endif  // CC_TEST_SKIA_COMMON_H_
