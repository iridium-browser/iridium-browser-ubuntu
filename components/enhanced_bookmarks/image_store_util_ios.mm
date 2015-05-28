// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/image_store_util.h"

#import <UIKit/UIKit.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_analysis.h"

namespace {
// An implementation of RefCountedMemory, where the bytes are stored in a
// NSData. This class assumes the NSData is non mutable to avoid a copy.
class RefCountedNSDataMemory : public base::RefCountedMemory {
 public:
  explicit RefCountedNSDataMemory(NSData* memory) : data_([memory retain]) {}

  const unsigned char* front() const override {
    return reinterpret_cast<const unsigned char*>([data_ bytes]);
  }

  size_t size() const override { return [data_ length]; }

private:
 ~RefCountedNSDataMemory() override {}

  base::scoped_nsobject<NSData> data_;
  DISALLOW_COPY_AND_ASSIGN(RefCountedNSDataMemory);
};
}  // namespace

namespace enhanced_bookmarks {

// Encodes the UIImage representation of a gfx::Image.
scoped_refptr<base::RefCountedMemory> BytesForImage(const gfx::Image& image) {
  DCHECK(image.HasRepresentation(gfx::Image::kImageRepCocoaTouch));
  return scoped_refptr<RefCountedNSDataMemory>(new RefCountedNSDataMemory(
      [NSKeyedArchiver archivedDataWithRootObject:image.ToUIImage()]));
}

// Decodes the UIImage in the bytes and returns a gfx::Image.
gfx::Image ImageForBytes(const scoped_refptr<base::RefCountedMemory>& data) {
  return gfx::Image([[NSKeyedUnarchiver unarchiveObjectWithData:
      [NSData dataWithBytes:data->front() length:data->size()]] retain]);
}

SkColor DominantColorForImage(const gfx::Image& image) {
  return color_utils::CalculateKMeanColorOfBitmap(*image.ToSkBitmap());
}

}  // namespace enhanced_bookmarks
