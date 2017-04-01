/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/image-decoders/ImageFrame.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "platform/image-decoders/ImageDecoder.h"

namespace blink {

ImageFrame::ImageFrame()
    : m_allocator(0),
      m_hasAlpha(true),
      m_status(FrameEmpty),
      m_duration(0),
      m_disposalMethod(DisposeNotSpecified),
      m_alphaBlendSource(BlendAtopPreviousFrame),
      m_premultiplyAlpha(true),
      m_pixelsChanged(false),
      m_requiredPreviousFrameIndex(kNotFound) {}

ImageFrame& ImageFrame::operator=(const ImageFrame& other) {
  if (this == &other)
    return *this;

  m_bitmap = other.m_bitmap;
  // Keep the pixels locked since we will be writing directly into the
  // bitmap throughout this object's lifetime.
  m_bitmap.lockPixels();
  // Be sure to assign this before calling setStatus(), since setStatus() may
  // call notifyBitmapIfPixelsChanged().
  m_pixelsChanged = other.m_pixelsChanged;
  setMemoryAllocator(other.allocator());
  setOriginalFrameRect(other.originalFrameRect());
  setStatus(other.getStatus());
  setDuration(other.duration());
  setDisposalMethod(other.getDisposalMethod());
  setAlphaBlendSource(other.getAlphaBlendSource());
  setPremultiplyAlpha(other.premultiplyAlpha());
  // Be sure that this is called after we've called setStatus(), since we
  // look at our status to know what to do with the alpha value.
  setHasAlpha(other.hasAlpha());
  setRequiredPreviousFrameIndex(other.requiredPreviousFrameIndex());
  return *this;
}

void ImageFrame::clearPixelData() {
  m_bitmap.reset();
  m_status = FrameEmpty;
  // NOTE: Do not reset other members here; clearFrameBufferCache()
  // calls this to free the bitmap data, but other functions like
  // initFrameBuffer() and frameComplete() may still need to read
  // other metadata out of this frame later.
}

void ImageFrame::zeroFillPixelData() {
  m_bitmap.eraseARGB(0, 0, 0, 0);
  m_hasAlpha = true;
}

bool ImageFrame::copyBitmapData(const ImageFrame& other) {
  DCHECK_NE(this, &other);
  m_hasAlpha = other.m_hasAlpha;
  m_bitmap.reset();
  return other.m_bitmap.copyTo(&m_bitmap, other.m_bitmap.colorType());
}

bool ImageFrame::takeBitmapDataIfWritable(ImageFrame* other) {
  DCHECK(other);
  DCHECK_EQ(FrameComplete, other->m_status);
  DCHECK_EQ(FrameEmpty, m_status);
  DCHECK_NE(this, other);
  if (other->m_bitmap.isImmutable())
    return false;
  m_hasAlpha = other->m_hasAlpha;
  m_bitmap.reset();
  m_bitmap.swap(other->m_bitmap);
  other->m_status = FrameEmpty;
  return true;
}

bool ImageFrame::setSizeAndColorSpace(int newWidth,
                                      int newHeight,
                                      sk_sp<SkColorSpace> colorSpace) {
  // setSizeAndColorSpace() should only be called once, it leaks memory
  // otherwise.
  DCHECK(!width() && !height());

  m_bitmap.setInfo(SkImageInfo::MakeN32(
      newWidth, newHeight,
      m_premultiplyAlpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType,
      std::move(colorSpace)));
  if (!m_bitmap.tryAllocPixels(m_allocator, 0))
    return false;

  zeroFillPixelData();
  return true;
}

bool ImageFrame::hasAlpha() const {
  return m_hasAlpha;
}

sk_sp<SkImage> ImageFrame::finalizePixelsAndGetImage() {
  DCHECK_EQ(FrameComplete, m_status);
  m_bitmap.setImmutable();
  return SkImage::MakeFromBitmap(m_bitmap);
}

void ImageFrame::setHasAlpha(bool alpha) {
  m_hasAlpha = alpha;

  m_bitmap.setAlphaType(computeAlphaType());
}

void ImageFrame::setStatus(Status status) {
  m_status = status;
  if (m_status == FrameComplete) {
    m_bitmap.setAlphaType(computeAlphaType());
    // Send pending pixels changed notifications now, because we can't do
    // this after the bitmap has been marked immutable.  We don't set the
    // bitmap immutable here because it would defeat
    // takeBitmapDataIfWritable().  Instead we let the bitmap stay mutable
    // until someone calls finalizePixelsAndGetImage() to actually get the
    // SkImage.
    notifyBitmapIfPixelsChanged();
  }
}

void ImageFrame::zeroFillFrameRect(const IntRect& rect) {
  if (rect.isEmpty())
    return;

  m_bitmap.eraseArea(rect, SkColorSetARGB(0, 0, 0, 0));
  setHasAlpha(true);
}

static uint8_t blendChannel(uint8_t src,
                            uint8_t srcA,
                            uint8_t dst,
                            uint8_t dstA,
                            unsigned scale) {
  unsigned blendUnscaled = src * srcA + dst * dstA;
  DCHECK(blendUnscaled < (1ULL << 32) / scale);
  return (blendUnscaled * scale) >> 24;
}

static uint32_t blendSrcOverDstNonPremultiplied(uint32_t src, uint32_t dst) {
  uint8_t srcA = SkGetPackedA32(src);
  if (srcA == 0)
    return dst;

  uint8_t dstA = SkGetPackedA32(dst);
  uint8_t dstFactorA = (dstA * SkAlpha255To256(255 - srcA)) >> 8;
  DCHECK(srcA + dstFactorA < (1U << 8));
  uint8_t blendA = srcA + dstFactorA;
  unsigned scale = (1UL << 24) / blendA;

  uint8_t blendR = blendChannel(SkGetPackedR32(src), srcA, SkGetPackedR32(dst),
                                dstFactorA, scale);
  uint8_t blendG = blendChannel(SkGetPackedG32(src), srcA, SkGetPackedG32(dst),
                                dstFactorA, scale);
  uint8_t blendB = blendChannel(SkGetPackedB32(src), srcA, SkGetPackedB32(dst),
                                dstFactorA, scale);

  return SkPackARGB32NoCheck(blendA, blendR, blendG, blendB);
}

void ImageFrame::blendRGBARaw(PixelData* dest,
                              unsigned r,
                              unsigned g,
                              unsigned b,
                              unsigned a) {
  *dest =
      blendSrcOverDstNonPremultiplied(SkPackARGB32NoCheck(a, r, g, b), *dest);
}

void ImageFrame::blendSrcOverDstRaw(PixelData* src, PixelData dst) {
  *src = blendSrcOverDstNonPremultiplied(*src, dst);
}

SkAlphaType ImageFrame::computeAlphaType() const {
  // If the frame is not fully loaded, there will be transparent pixels,
  // so we can't tell skia we're opaque, even for image types that logically
  // always are (e.g. jpeg).
  if (!m_hasAlpha && m_status == FrameComplete)
    return kOpaque_SkAlphaType;

  return m_premultiplyAlpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
}

}  // namespace blink
