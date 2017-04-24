/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AcceleratedImageBufferSurface_h
#define AcceleratedImageBufferSurface_h

#include "platform/graphics/ImageBufferSurface.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "platform/graphics/paint/PaintSurface.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include <memory>

namespace blink {

class PLATFORM_EXPORT AcceleratedImageBufferSurface
    : public ImageBufferSurface {
  WTF_MAKE_NONCOPYABLE(AcceleratedImageBufferSurface);
  USING_FAST_MALLOC(AcceleratedImageBufferSurface);

 public:
  AcceleratedImageBufferSurface(const IntSize&,
                                OpacityMode = NonOpaque,
                                sk_sp<SkColorSpace> = nullptr,
                                SkColorType = kN32_SkColorType);
  ~AcceleratedImageBufferSurface() override {}

  PaintCanvas* canvas() override {
    return m_surface ? m_surface->getCanvas() : nullptr;
  }
  bool isValid() const override;
  bool isAccelerated() const override { return true; }
  sk_sp<SkImage> newImageSnapshot(AccelerationHint, SnapshotReason) override;
  GLuint getBackingTextureHandleForOverwrite() override;

 private:
  unsigned m_contextId;
  sk_sp<PaintSurface> m_surface;  // Uses m_contextProvider.
};

}  // namespace blink

#endif
