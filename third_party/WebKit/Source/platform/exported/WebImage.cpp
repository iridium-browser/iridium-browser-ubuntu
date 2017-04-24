/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "public/platform/WebImage.h"

#include "platform/SharedBuffer.h"
#include "platform/graphics/Image.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "public/platform/WebData.h"
#include "public/platform/WebSize.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include <algorithm>
#include <memory>

namespace blink {

WebImage WebImage::fromData(const WebData& data, const WebSize& desiredSize) {
  RefPtr<SharedBuffer> buffer = PassRefPtr<SharedBuffer>(data);
  std::unique_ptr<ImageDecoder> decoder(ImageDecoder::create(
      buffer, true, ImageDecoder::AlphaPremultiplied, ColorBehavior::ignore()));
  if (!decoder || !decoder->isSizeAvailable())
    return WebImage();

  // Frames are arranged by decreasing size, then decreasing bit depth.
  // Pick the frame closest to |desiredSize|'s area without being smaller,
  // which has the highest bit depth.
  const size_t frameCount = decoder->frameCount();
  size_t index = 0;  // Default to first frame if none are large enough.
  int frameAreaAtIndex = 0;
  for (size_t i = 0; i < frameCount; ++i) {
    const IntSize frameSize = decoder->frameSizeAtIndex(i);
    if (WebSize(frameSize) == desiredSize) {
      index = i;
      break;  // Perfect match.
    }

    const int frameArea = frameSize.width() * frameSize.height();
    if (frameArea < (desiredSize.width * desiredSize.height))
      break;  // No more frames that are large enough.

    if (!i || (frameArea < frameAreaAtIndex)) {
      index = i;  // Closer to desired area than previous best match.
      frameAreaAtIndex = frameArea;
    }
  }

  ImageFrame* frame = decoder->frameBufferAtIndex(index);
  return (frame && !decoder->failed()) ? WebImage(frame->bitmap()) : WebImage();
}

WebVector<WebImage> WebImage::framesFromData(const WebData& data) {
  // This is to protect from malicious images. It should be big enough that it's
  // never hit in practice.
  const size_t maxFrameCount = 8;

  RefPtr<SharedBuffer> buffer = PassRefPtr<SharedBuffer>(data);
  std::unique_ptr<ImageDecoder> decoder(ImageDecoder::create(
      buffer, true, ImageDecoder::AlphaPremultiplied, ColorBehavior::ignore()));
  if (!decoder || !decoder->isSizeAvailable())
    return WebVector<WebImage>();

  // Frames are arranged by decreasing size, then decreasing bit depth.
  // Keep the first frame at every size, has the highest bit depth.
  const size_t frameCount = decoder->frameCount();
  IntSize lastSize;

  Vector<WebImage> frames;
  for (size_t i = 0; i < std::min(frameCount, maxFrameCount); ++i) {
    const IntSize frameSize = decoder->frameSizeAtIndex(i);
    if (frameSize == lastSize)
      continue;
    lastSize = frameSize;

    ImageFrame* frame = decoder->frameBufferAtIndex(i);
    if (!frame)
      continue;

    SkBitmap bitmap = frame->bitmap();
    if (!bitmap.isNull() && frame->getStatus() == ImageFrame::FrameComplete)
      frames.push_back(WebImage(bitmap));
  }

  return frames;
}

void WebImage::reset() {
  m_bitmap.reset();
}

void WebImage::assign(const WebImage& image) {
  m_bitmap = image.m_bitmap;
}

bool WebImage::isNull() const {
  return m_bitmap.isNull();
}

WebSize WebImage::size() const {
  return WebSize(m_bitmap.width(), m_bitmap.height());
}

WebImage::WebImage(PassRefPtr<Image> image) {
  if (!image)
    return;

  // TODO(ccameron): WebImage needs to be consistent about color spaces.
  // https://crbug.com/672315
  if (sk_sp<SkImage> skImage =
          image->imageForCurrentFrame(ColorBehavior::transformToGlobalTarget()))
    skImage->asLegacyBitmap(&m_bitmap, SkImage::kRO_LegacyBitmapMode);
}

}  // namespace blink
